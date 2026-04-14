#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_LEVELUP
// ---------------------------------------------------------------------------
static Window  *s_lu_window  = NULL;
static Layer   *s_lu_layer   = NULL;
static Pet      s_lu_pet;
static uint8_t  s_lu_row     = 0;
static uint16_t s_lu_alloc[NUM_STATS];

static void lu_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  graphics_draw_text(ctx, "LEVEL UP!",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 24, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Happy fox
  ui_draw_fox(ctx, GPoint(w - 24, 30), FOX_HAPPY, 0);

  char pts_buf[20];
  snprintf(pts_buf, sizeof(pts_buf), "Points: %d", (int)s_lu_pet.upgrade_points);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, pts_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, 44, w, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  for (uint8_t i = 0; i < NUM_STATS; i++) {
    int16_t row_y = 64 + (int16_t)(i * 16);
    bool sel = (s_lu_row == i);
    if (sel) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(0, row_y - 1, w, 15), 0, GCornerNone);
    }
    char sbuf[20];
    snprintf(sbuf, sizeof(sbuf), "%s: %d (+%d)",
             s_stat_names[i], (int)pet_get_stat(&s_lu_pet, i), (int)s_lu_alloc[i]);
    graphics_context_set_text_color(ctx,
      sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorWhite);
    graphics_draw_text(ctx, sbuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(4, row_y, w - 8, 14),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  bool done_sel = (s_lu_row == NUM_STATS);
  graphics_context_set_text_color(ctx,
    done_sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorLightGray);
  graphics_draw_text(ctx, done_sel ? "> DONE <" : "  DONE  ",
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, 64 + NUM_STATS * 16, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void lu_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row > 0) s_lu_row--;
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row < NUM_STATS) s_lu_row++;
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row < NUM_STATS) {
    uint16_t val = pet_get_stat(&s_lu_pet, s_lu_row);
    if (stats_raise_stat(&val, &s_lu_pet.upgrade_points)) {
      pet_set_stat(&s_lu_pet, s_lu_row, val);
      s_lu_alloc[s_lu_row]++;
    }
  } else {
    pet_save(&s_lu_pet);
    window_stack_pop(true);
    screens_push_main();
  }
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row < NUM_STATS && s_lu_alloc[s_lu_row] > 0) {
    uint16_t val = pet_get_stat(&s_lu_pet, s_lu_row);
    if (stats_lower_stat(&val, &s_lu_pet.upgrade_points)) {
      pet_set_stat(&s_lu_pet, s_lu_row, val);
      s_lu_alloc[s_lu_row]--;
    }
  }
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,    lu_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,  lu_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, lu_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK,   lu_click_back);
}

static void lu_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_lu_layer = layer_create(bounds);
  layer_set_update_proc(s_lu_layer, lu_layer_update);
  layer_add_child(root, s_lu_layer);
  window_set_click_config_provider(window, lu_click_config);
}

static void lu_window_unload(Window *window) {
  layer_destroy(s_lu_layer); s_lu_layer = NULL;
  window_destroy(s_lu_window); s_lu_window = NULL;
}

void screens_push_levelup(Pet *pet) {
  s_lu_pet = *pet;
  s_lu_row = 0;
  memset(s_lu_alloc, 0, sizeof(s_lu_alloc));

  s_lu_window = window_create();
  window_set_background_color(s_lu_window, GColorBlack);
  window_set_window_handlers(s_lu_window, (WindowHandlers) {
    .load   = lu_window_load,
    .unload = lu_window_unload
  });
  window_stack_push(s_lu_window, true);
}
