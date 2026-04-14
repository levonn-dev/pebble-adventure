#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../events.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_MAIN
// ---------------------------------------------------------------------------
Window   *s_main_window     = NULL;
Layer    *s_main_layer      = NULL;
static AppTimer *s_main_anim_timer = NULL;
static uint8_t   s_main_fox_frame  = 0;
static Pet       s_main_pet;
bool      s_main_has_active_adv = false;

static void main_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  ui_draw_time(ctx, bounds);
  ui_draw_battery(ctx, bounds);

  // Fox in upper quarter
  GPoint fox_center = GPoint(w / 2, h / 4 + 4);
  ui_draw_fox(ctx, fox_center, FOX_IDLE, s_main_fox_frame);

  // Name + level
  char name_buf[20];
  snprintf(name_buf, sizeof(name_buf), "%s  Lv.%d",
           s_main_pet.name, (int)s_main_pet.level);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, name_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, h / 4 + 20, w, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // XP bar
  ui_draw_progress_bar(ctx,
    GRect(8, h / 4 + 40, w - 16, 8),
    s_main_pet.xp, s_main_pet.xp_next_level);

  // Stats -- 2 columns
  static const char *labels[NUM_STATS] = { "STR", "DEX", "AGI", "VIT", "INT", "LUK" };
  int16_t stats_top = h / 4 + 54;
  for (int i = 0; i < NUM_STATS; i++) {
    int16_t col_x = (i % 2 == 0) ? 4 : w / 2 + 4;
    int16_t row_y = stats_top + (int16_t)(i / 2) * 17;
    char sbuf[12];
    snprintf(sbuf, sizeof(sbuf), "%s:%d", labels[i], (int)pet_get_stat(&s_main_pet, i));
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, sbuf,
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(col_x, row_y, w / 2 - 6, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Bottom action hints
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, s_main_has_active_adv ? "SEL: Resume" : "SEL: Adventure",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 18, w / 2, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "UP: Options",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(w / 2, h - 18, w / 2, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void main_anim_callback(void *ctx) {
  s_main_fox_frame = (s_main_fox_frame + 1) % 4;
  layer_mark_dirty(s_main_layer);
  s_main_anim_timer = app_timer_register(600, main_anim_callback, NULL);
}

static void main_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  Adventure adv;
  s_main_has_active_adv = adventure_load(&adv) && adv.active;
  if (!s_main_has_active_adv) {
    pet_load(&s_main_pet);
    adventure_init(&adv, &s_main_pet);
    adventure_save(&adv);
  }
  screens_push_adventure();
}

// ---------------------------------------------------------------------------
// OPTIONS MENU (pushed from main screen Up button)
// ---------------------------------------------------------------------------
static Window  *s_opt_window  = NULL;
static Layer   *s_opt_layer   = NULL;
static uint8_t  s_opt_cursor  = 0;   // 0=Vibration, 1=Delete Pet
static bool     s_opt_confirm = false;

#define OPT_VIBRATION 0
#define OPT_DELETE    1
#ifdef DEBUG_MODE
#define OPT_SKIP_SEG  2
#define OPT_COUNT     3
#else
#define OPT_COUNT     2
#endif

static void opt_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "OPTIONS",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 26, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (s_opt_confirm) {
    // Delete confirmation screen
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "DELETE FOX?",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, h / 2 - 28, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    char name_buf[20];
    snprintf(name_buf, sizeof(name_buf), "\"%s\" Lv.%d", s_main_pet.name, (int)s_main_pet.level);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, name_buf,
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 - 4, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_draw_text(ctx, "This cannot be undone!",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 + 14, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "SELECT: Delete",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, h - 36, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "BACK: Cancel",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h - 18, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    // Menu items
    // Vibration toggle
    int16_t row0_y = 56;
    GColor c0 = ui_draw_menu_row(ctx, row0_y, w, 18, s_opt_cursor == OPT_VIBRATION);
    char vib_buf[20];
    snprintf(vib_buf, sizeof(vib_buf), "Vibration: %s", ui_vibration_enabled() ? "ON" : "OFF");
    graphics_context_set_text_color(ctx, c0);
    graphics_draw_text(ctx, vib_buf,
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row0_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Delete pet
    int16_t row1_y = row0_y + 24;
    GColor c1 = ui_draw_menu_row(ctx, row1_y, w, 18, s_opt_cursor == OPT_DELETE);
    (void)c1;
    graphics_context_set_text_color(ctx,
      s_opt_cursor == OPT_DELETE ? PBL_IF_COLOR_ELSE(GColorRed, GColorBlack) : PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "Delete Pet",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row1_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

#ifdef DEBUG_MODE
    // Debug: skip segment
    int16_t row2_y = row1_y + 24;
    GColor c2 = ui_draw_menu_row(ctx, row2_y, w, 18, s_opt_cursor == OPT_SKIP_SEG);
    graphics_context_set_text_color(ctx, c2);
    graphics_draw_text(ctx, "[DBG] Skip Seg",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row2_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
#endif

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "BACK: Return",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h - 18, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void opt_deferred_remove_both(void *data) {
  (void)data;
  if (s_opt_window) window_stack_remove(s_opt_window, false);
  if (s_main_window) window_stack_remove(s_main_window, false);
}

static void opt_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_opt_confirm && s_opt_cursor > 0) s_opt_cursor--;
  layer_mark_dirty(s_opt_layer);
}

static void opt_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_opt_confirm && s_opt_cursor < OPT_COUNT - 1) s_opt_cursor++;
  layer_mark_dirty(s_opt_layer);
}

static void opt_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_opt_confirm) {
    // Confirmed delete
    persist_delete(PERSIST_KEY_PET);
    persist_delete(PERSIST_KEY_ADVENTURE);
    persist_delete(PERSIST_KEY_WORKER_STEPS);
    persist_delete(PERSIST_KEY_PENDING_ENCOUNTER);
    screens_push_creation();
    app_timer_register(50, opt_deferred_remove_both, NULL);
    return;
  }

  if (s_opt_cursor == OPT_VIBRATION) {
    ui_set_vibration(!ui_vibration_enabled());
    layer_mark_dirty(s_opt_layer);
  } else if (s_opt_cursor == OPT_DELETE) {
    s_opt_confirm = true;
    layer_mark_dirty(s_opt_layer);
  }
#ifdef DEBUG_MODE
  else if (s_opt_cursor == OPT_SKIP_SEG) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active && adv.current_segment < adv.num_segments) {
      // Complete current segment
      adv.segment_progress[adv.current_segment] = adv.segment_length[adv.current_segment];
      uint8_t diff = adv.segments[adv.current_segment] + 1;
      adv.total_xp_earned += (uint32_t)(adv.current_segment * 50) + (uint32_t)(diff * 30);
      adv.current_segment++;
      if (adv.current_segment >= adv.num_segments) {
        adv.active = false;
      }
      adventure_save(&adv);
      s_main_has_active_adv = adv.active;
    }
    layer_mark_dirty(s_opt_layer);
  }
#endif
}

static void opt_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_opt_confirm) {
    s_opt_confirm = false;
    layer_mark_dirty(s_opt_layer);
  } else {
    window_stack_pop(true);
  }
}

static void opt_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     opt_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   opt_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, opt_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK,   opt_click_back);
}

static void opt_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_opt_layer = layer_create(bounds);
  layer_set_update_proc(s_opt_layer, opt_layer_update);
  layer_add_child(root, s_opt_layer);
  window_set_click_config_provider(window, opt_click_config);
}

static void opt_deferred_destroy(void *data) {
  Window *w = (Window *)data;
  if (w) window_destroy(w);
}

static void opt_window_unload(Window *window) {
  layer_destroy(s_opt_layer); s_opt_layer = NULL;
  app_timer_register(50, opt_deferred_destroy, s_opt_window);
  s_opt_window = NULL;
}

static void main_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  s_opt_confirm = false;
  s_opt_cursor = 0;
  s_opt_window = window_create();
  window_set_background_color(s_opt_window, GColorBlack);
  window_set_window_handlers(s_opt_window, (WindowHandlers) {
    .load   = opt_window_load,
    .unload = opt_window_unload
  });
  window_stack_push(s_opt_window, true);
}

static void main_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     main_click_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, main_click_select);
}

static void main_window_appear(Window *window) {
  // Refresh state when returning from adventure/options/stat alloc
  pet_load(&s_main_pet);
  Adventure adv;
  s_main_has_active_adv = adventure_load(&adv) && adv.active;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void main_window_load(Window *window) {
  pet_load(&s_main_pet);
  Adventure adv;
  s_main_has_active_adv = adventure_load(&adv) && adv.active;
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_main_layer = layer_create(bounds);
  layer_set_update_proc(s_main_layer, main_layer_update);
  layer_add_child(root, s_main_layer);
  window_set_click_config_provider(window, main_click_config);
  s_main_anim_timer = app_timer_register(600, main_anim_callback, NULL);
}

static void main_deferred_destroy(void *data) {
  Window *w = (Window *)data;
  if (w) window_destroy(w);
}

static void main_window_unload(Window *window) {
  if (s_main_anim_timer) { app_timer_cancel(s_main_anim_timer); s_main_anim_timer = NULL; }
  layer_destroy(s_main_layer); s_main_layer = NULL;
  app_timer_register(50, main_deferred_destroy, s_main_window);
  s_main_window = NULL;
}

void screens_push_main(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .appear = main_window_appear,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
}
