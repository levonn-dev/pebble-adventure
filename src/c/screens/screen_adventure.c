#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../events.h"
#include "../stats.h"
#include "../ui.h"
#include "../minigames.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_ADVENTURE
// ---------------------------------------------------------------------------
static Window    *s_adv_window     = NULL;
Layer     *s_adv_layer      = NULL;
static AppTimer  *s_adv_anim_timer = NULL;
static uint8_t    s_adv_fox_frame  = 0;
Adventure  s_adv_current;
static Pet        s_adv_pet;
// Encounter popup queue
#define POPUP_QUEUE_MAX 8
static uint8_t s_adv_popup_count = 0;
static uint8_t s_adv_popup_index = 0;  // currently displayed
static struct {
  char name[16];    // encounter name
  char effect[20];  // "+10% progress" etc
  bool won;
} s_adv_popups[POPUP_QUEUE_MAX];

static const char *s_biome_names[NUM_BIOMES] = {
  "Plains", "Forest", "Water", "Mountain", "Cave", "Storm"
};

void adv_queue_popup(const char *title, const char *detail, bool won) {
  if (s_adv_popup_count < POPUP_QUEUE_MAX) {
    uint8_t i = s_adv_popup_count;
    snprintf(s_adv_popups[i].name, sizeof(s_adv_popups[i].name), "%s", title);
    snprintf(s_adv_popups[i].effect, sizeof(s_adv_popups[i].effect), "%s", detail);
    s_adv_popups[i].won = won;
    s_adv_popup_count++;
  }
  if (s_adv_layer) layer_mark_dirty(s_adv_layer);
}

void adv_resolve_pending_encounter(void) {
  if (!persist_exists(PERSIST_KEY_PENDING_ENCOUNTER)) return;
  int32_t enc_id = persist_read_int(PERSIST_KEY_PENDING_ENCOUNTER);
  if (enc_id < 0 || enc_id >= NUM_ENCOUNTERS) {
    persist_delete(PERSIST_KEY_PENDING_ENCOUNTER);
    return;
  }

  Pet pet;
  pet_load(&pet);
  EncounterResult result = encounter_resolve((uint8_t)enc_id, &pet);
  encounter_apply(&result, &s_adv_current);
  adventure_save(&s_adv_current);

  // Format and queue popup
  char detail[20];
  if (result.progress_change != 0) {
    snprintf(detail, sizeof(detail), "%s%d%% progress",
             result.progress_change > 0 ? "+" : "", (int)result.progress_change);
  } else if (result.bonus_xp > 0) {
    snprintf(detail, sizeof(detail), "+%lu XP", (unsigned long)result.bonus_xp);
  } else {
    snprintf(detail, sizeof(detail), "No effect");
  }
  adv_queue_popup(result.encounter_name, detail, result.won);

  persist_delete(PERSIST_KEY_PENDING_ENCOUNTER);
  if (s_adv_layer) layer_mark_dirty(s_adv_layer);
}

static void adv_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Time -- always visible top-left
  ui_draw_time(ctx, bounds);

  // --- IDLE MODE: no active adventure ---
  if (!s_adv_current.active && !adventure_is_complete(&s_adv_current)) {
    ui_draw_battery(ctx, bounds);

    // Fox idle animation (centered)
    GPoint fox_center = GPoint(w / 2, h / 2 - 10);
    ui_draw_fox(ctx, fox_center, FOX_IDLE, s_adv_fox_frame);

    // Name + level
    char name_buf[20];
    snprintf(name_buf, sizeof(name_buf), "%s  Lv.%d",
             s_adv_pet.name, (int)s_adv_pet.level);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, name_buf,
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, h / 2 + 10, w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // XP bar
    ui_draw_progress_bar(ctx,
      GRect(8, h / 2 + 30, w - 16, 8),
      s_adv_pet.xp, s_adv_pet.xp_next_level);

    // Button hints
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "UP:stats  SEL:menu  DN:adventure",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h - 18, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // --- COMPLETE MODE ---
  if (adventure_is_complete(&s_adv_current)) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, "ADVENTURE DONE!",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, h / 2 - 22, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "SELECT: Results",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 + 6, w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // --- ACTIVE MODE: adventure in progress ---
  uint8_t seg = s_adv_current.current_segment;

  // Segment counter top-right
  char seg_buf[12];
  snprintf(seg_buf, sizeof(seg_buf), "Seg %d/%d", (int)(seg + 1), (int)s_adv_current.num_segments);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, seg_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(w - 62, 2, 60, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Progress bar
  uint8_t pct = adventure_segment_progress_pct(&s_adv_current);
  ui_draw_progress_bar(ctx, GRect(8, 22, w - 16, 10), (uint32_t)pct, 100);

  // Biome + pct label
  const char *bname = (s_adv_current.segments[seg] < NUM_BIOMES) ? s_biome_names[s_adv_current.segments[seg]] : "?";
  char biome_buf[20];
  snprintf(biome_buf, sizeof(biome_buf), "%s  %d%%", bname, (int)pct);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
  graphics_draw_text(ctx, biome_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, 34, w - 8, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Biome background
  int16_t ground_y = h / 2 + 8;
  GRect biome_area = GRect(0, 50, w, ground_y - 50 + (h - ground_y));
  ui_draw_biome_bg(ctx, biome_area, s_adv_current.segments[seg], s_adv_fox_frame);

  // Walking fox
  ui_draw_fox(ctx, GPoint(w / 3, ground_y - 12), FOX_WALK, s_adv_fox_frame);

  // Dark strip behind bottom text for readability on B&W
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, h - 36, w, 36), 0, GCornerNone);

  // Steps today
  uint32_t steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);
  char steps_buf[20];
  snprintf(steps_buf, sizeof(steps_buf), "Steps: %lu", (unsigned long)steps);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, steps_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, h - 34, w - 8, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Button hints
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "UP:stats  SEL:menu  DN:games",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 18, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Encounter popup queue
  if (s_adv_popup_index < s_adv_popup_count) {
    GRect popup = GRect(4, h / 2 - (34 - ((s_adv_popup_count > 1) * 6)), w - 8, 64);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
    graphics_fill_rect(ctx, popup, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_round_rect(ctx, popup, 4);

    ui_draw_fox(ctx, GPoint(16, h / 2 - 12),
                s_adv_popups[s_adv_popup_index].won ? FOX_HAPPY : FOX_SAD, 0);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, s_adv_popups[s_adv_popup_index].name,
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(36, h / 2 - 30, w - 44, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    graphics_context_set_text_color(ctx,
      s_adv_popups[s_adv_popup_index].won
        ? PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite)
        : PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx,
      s_adv_popups[s_adv_popup_index].won ? "Won!" : "Lost",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(36, h / 2 - 14, w - 44, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_adv_popups[s_adv_popup_index].effect,
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(36, h / 2 + 2, w - 44, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    if (s_adv_popup_count > 1) {
      char qbuf[8];
      snprintf(qbuf, sizeof(qbuf), "%d/%d", (int)(s_adv_popup_index + 1), (int)s_adv_popup_count);
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, qbuf,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(36, h / 2 + 18, w - 44, 14),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }
}

static void adv_anim_callback(void *ctx) {
  s_adv_fox_frame = (s_adv_fox_frame + 1) % 4;
  layer_mark_dirty(s_adv_layer);
  uint32_t interval = s_adv_current.active ? 300 : 600;
  s_adv_anim_timer = app_timer_register(interval, adv_anim_callback, NULL);
}

// Advance popup queue or dismiss
static bool adv_dismiss_popup(void) {
  if (s_adv_popup_index < s_adv_popup_count) {
    s_adv_popup_index++;
    if (s_adv_popup_index >= s_adv_popup_count) {
      // All popups seen — reset queue
      s_adv_popup_count = 0;
      s_adv_popup_index = 0;
    }
    layer_mark_dirty(s_adv_layer);
    return true;
  }
  return false;
}

static void adv_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (adv_dismiss_popup()) return;
  screens_push_stats();
}

static void adv_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (adv_dismiss_popup()) return;
  if (adventure_is_complete(&s_adv_current)) {
    window_stack_pop(true);
    screens_push_results(s_adv_current.total_xp_earned, s_adv_current.encounters_total);
  } else {
    screens_push_menu();
  }
}

static void adv_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (adv_dismiss_popup()) return;
  if (s_adv_current.active) {
    if (!adventure_is_complete(&s_adv_current)) {
      minigames_push_selection();
    }
  } else {
    // Start new adventure
    Pet pet;
    pet_load(&pet);
    Adventure adv;
    adventure_init(&adv, &pet);
    adventure_save(&adv);
    adventure_load(&s_adv_current);
    pet_load(&s_adv_pet);
    layer_mark_dirty(s_adv_layer);
  }
}

static void adv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     adv_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   adv_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, adv_click_select);
}

static void adv_window_appear(Window *window) {
  adventure_load(&s_adv_current);
  pet_load(&s_adv_pet);
  if (s_adv_layer) layer_mark_dirty(s_adv_layer);
}

static void adv_window_load(Window *window) {
  adventure_load(&s_adv_current);
  pet_load(&s_adv_pet);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_adv_layer = layer_create(bounds);
  layer_set_update_proc(s_adv_layer, adv_layer_update);
  layer_add_child(root, s_adv_layer);
  window_set_click_config_provider(window, adv_click_config);
  s_adv_anim_timer = app_timer_register(300, adv_anim_callback, NULL);
}

static void adv_window_unload(Window *window) {
  if (s_adv_anim_timer) { app_timer_cancel(s_adv_anim_timer); s_adv_anim_timer = NULL; }
  layer_destroy(s_adv_layer); s_adv_layer = NULL;
  window_destroy(s_adv_window); s_adv_window = NULL;
}

void screens_push_adventure(void) {
  s_adv_window = window_create();
  window_set_background_color(s_adv_window, GColorBlack);
  window_set_window_handlers(s_adv_window, (WindowHandlers) {
    .load   = adv_window_load,
    .appear = adv_window_appear,
    .unload = adv_window_unload
  });
  window_stack_push(s_adv_window, true);
}
