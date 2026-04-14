#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_STATS — display-only pet information
// ---------------------------------------------------------------------------
static Window   *s_st_window     = NULL;
static Layer    *s_st_layer      = NULL;
static AppTimer *s_st_anim_timer = NULL;
static uint8_t   s_st_fox_frame  = 0;
static Pet       s_st_pet;

static void st_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  ui_draw_status_bar(ctx, bounds);

  // Fox in upper quarter
  GPoint fox_center = GPoint(w / 2, h / 4 + 4);
  ui_draw_fox(ctx, fox_center, FOX_IDLE, s_st_fox_frame);

  // Name + level
  char name_buf[20];
  snprintf(name_buf, sizeof(name_buf), "%s  Lv.%d",
           s_st_pet.name, (int)s_st_pet.level);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, name_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, h / 4 + 20, w, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // XP bar
  ui_draw_progress_bar(ctx,
    GRect(8, h / 4 + 40, w - 16, 8),
    s_st_pet.xp, s_st_pet.xp_next_level);

  // Stats -- 2 columns
  static const char *labels[NUM_STATS] = { "STR", "DEX", "AGI", "VIT", "INT", "LUK" };
  int16_t stats_top = h / 4 + 54;
  for (int i = 0; i < NUM_STATS; i++) {
    int16_t col_x = (i % 2 == 0) ? 4 : w / 2 + 4;
    int16_t row_y = stats_top + (int16_t)(i / 2) * 17;
    char sbuf[12];
    snprintf(sbuf, sizeof(sbuf), "%s:%d", labels[i], (int)pet_get_stat(&s_st_pet, i));
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, sbuf,
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(col_x, row_y, w / 2 - 6, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void st_anim_callback(void *ctx) {
  s_st_fox_frame = (s_st_fox_frame + 1) % 4;
  layer_mark_dirty(s_st_layer);
  s_st_anim_timer = app_timer_register(600, st_anim_callback, NULL);
}

static void st_window_load(Window *window) {
  pet_load(&s_st_pet);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_st_layer = layer_create(bounds);
  layer_set_update_proc(s_st_layer, st_layer_update);
  layer_add_child(root, s_st_layer);
  // No click config — BACK is default behavior (pops window)
  s_st_anim_timer = app_timer_register(600, st_anim_callback, NULL);
}

static void st_window_unload(Window *window) {
  if (s_st_anim_timer) { app_timer_cancel(s_st_anim_timer); s_st_anim_timer = NULL; }
  layer_destroy(s_st_layer); s_st_layer = NULL;
  window_destroy(s_st_window); s_st_window = NULL;
}

void screens_push_stats(void) {
  s_st_fox_frame = 0;
  s_st_window = window_create();
  window_set_background_color(s_st_window, GColorBlack);
  window_set_window_handlers(s_st_window, (WindowHandlers) {
    .load   = st_window_load,
    .unload = st_window_unload
  });
  window_stack_push(s_st_window, true);
}
