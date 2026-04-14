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
static bool       s_adv_show_info  = false;
static uint8_t    s_adv_popup_ticks = 0;   // animation ticks remaining (0 = hidden)
static char       s_adv_popup_title[24];
static char       s_adv_popup_detail[32];
static bool       s_adv_popup_won = false;

static const char *s_biome_names[NUM_BIOMES] = {
  "Plains", "Forest", "Water", "Mountain", "Cave", "Storm"
};

void adv_queue_popup(const char *title, const char *detail, bool won) {
  snprintf(s_adv_popup_title, sizeof(s_adv_popup_title), "%s", title);
  snprintf(s_adv_popup_detail, sizeof(s_adv_popup_detail), "%s", detail);
  s_adv_popup_won = won;
  s_adv_popup_ticks = 10;
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

  snprintf(s_adv_popup_title, sizeof(s_adv_popup_title), "%s: %s",
           result.encounter_name, result.won ? "WIN" : "LOSE");
  if (result.progress_change != 0) {
    snprintf(s_adv_popup_detail, sizeof(s_adv_popup_detail), "%s%d%% progress",
             result.progress_change > 0 ? "+" : "", (int)result.progress_change);
  } else if (result.bonus_xp > 0) {
    snprintf(s_adv_popup_detail, sizeof(s_adv_popup_detail), "+%lu bonus XP",
             (unsigned long)result.bonus_xp);
  } else {
    snprintf(s_adv_popup_detail, sizeof(s_adv_popup_detail), "No effect");
  }
  s_adv_popup_ticks = 10;  // 10 ticks x 300ms = 3 seconds
  s_adv_popup_won = result.won;

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
  graphics_draw_text(ctx, "UP:info  DN:games",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 18, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Info overlay (toggled by Up) -- uses cached s_adv_pet, loaded on window_load
  if (s_adv_show_info) {
    GRect info = GRect(8, 50, w - 16, 68);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_fill_rect(ctx, info, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_round_rect(ctx, info, 4);

    char ibuf[32];
    snprintf(ibuf, sizeof(ibuf), "%s  Lv.%d", s_adv_pet.name, (int)s_adv_pet.level);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, ibuf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(12, 53, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    snprintf(ibuf, sizeof(ibuf), "XP %lu/%lu", (unsigned long)s_adv_pet.xp, (unsigned long)s_adv_pet.xp_next_level);
    graphics_draw_text(ctx, ibuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(12, 70, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    snprintf(ibuf, sizeof(ibuf), "S%d D%d A%d V%d I%d L%d",
             (int)s_adv_pet.str, (int)s_adv_pet.dex, (int)s_adv_pet.agi,
             (int)s_adv_pet.vit, (int)s_adv_pet.intel, (int)s_adv_pet.luk);
    graphics_draw_text(ctx, ibuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(12, 88, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Encounter popup overlay
  if (s_adv_popup_ticks > 0) {
    GRect popup = GRect(4, h / 2 - 24, w - 8, 48);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
    graphics_fill_rect(ctx, popup, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_round_rect(ctx, popup, 4);

    // Fox reaction
    ui_draw_fox(ctx, GPoint(20, h / 2 - 8),
                s_adv_popup_won ? FOX_HAPPY : FOX_SAD, 0);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, s_adv_popup_title,
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(36, h / 2 - 22, w - 44, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_adv_popup_detail,
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(36, h / 2 - 4, w - 44, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void adv_anim_callback(void *ctx) {
  s_adv_fox_frame = (s_adv_fox_frame + 1) % 4;
  if (s_adv_popup_ticks > 0) s_adv_popup_ticks--;
  // Don't poll persist here -- s_adv_current is refreshed by screens_on_worker_message
  layer_mark_dirty(s_adv_layer);
  s_adv_anim_timer = app_timer_register(300, adv_anim_callback, NULL);
}

static void adv_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  s_adv_show_info = !s_adv_show_info;
  layer_mark_dirty(s_adv_layer);
}

static void adv_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (adventure_is_complete(&s_adv_current)) {
    window_stack_pop(true);
    screens_push_results(s_adv_current.total_xp_earned, s_adv_current.encounters_total);
  }
}

static void adv_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!adventure_is_complete(&s_adv_current)) {
    minigames_push_selection();
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
  s_adv_show_info = false;
  s_adv_window = window_create();
  window_set_background_color(s_adv_window, GColorBlack);
  window_set_window_handlers(s_adv_window, (WindowHandlers) {
    .load   = adv_window_load,
    .appear = adv_window_appear,
    .unload = adv_window_unload
  });
  window_stack_push(s_adv_window, true);
}
