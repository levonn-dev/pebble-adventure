#include "../minigames.h"
#include "minigames_internal.h"
#include "../shared_types.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// MINI-GAME SELECTION
// ---------------------------------------------------------------------------
static Window *s_sel_window = NULL;
static Layer  *s_sel_layer  = NULL;
static uint8_t s_sel_cursor = 0;

static const char *s_game_names[4] = {
  "Dodge (AGI)", "Catch (DEX)", "Riddle (INT)", "Treasure (LUK)"
};

static void sel_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "MINI-GAMES",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 26, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  for (uint8_t i = 0; i < 4; i++) {
    int16_t row_y = 56 + (int16_t)(i * 22);
    bool sel = (s_sel_cursor == i);

    if (sel) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(0, row_y - 1, w, 20), 0, GCornerNone);
    }

    graphics_context_set_text_color(ctx,
      sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorWhite);
    graphics_draw_text(ctx, s_game_names[i],
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row_y, w - 16, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void sel_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sel_cursor > 0) s_sel_cursor--;
  layer_mark_dirty(s_sel_layer);
}

static void sel_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sel_cursor < 3) s_sel_cursor++;
  layer_mark_dirty(s_sel_layer);
}

static void sel_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  uint8_t choice = s_sel_cursor;
  window_stack_pop(false);
  switch (choice) {
    case 0: minigame_push_dodge();    break;
    case 1: minigame_push_catch();    break;
    case 2: minigame_push_riddle();   break;
    case 3: minigame_push_treasure(); break;
  }
}

static void sel_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     sel_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   sel_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, sel_click_select);
}

static void sel_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_sel_layer = layer_create(bounds);
  layer_set_update_proc(s_sel_layer, sel_layer_update);
  layer_add_child(root, s_sel_layer);
  window_set_click_config_provider(window, sel_click_config);
}

static void sel_window_unload(Window *window) {
  layer_destroy(s_sel_layer); s_sel_layer = NULL;
  window_destroy(s_sel_window); s_sel_window = NULL;
}

void minigames_push_selection(void) {
  s_sel_cursor = 0;
  s_sel_window = window_create();
  window_set_background_color(s_sel_window, GColorBlack);
  window_set_window_handlers(s_sel_window, (WindowHandlers) {
    .load   = sel_window_load,
    .unload = sel_window_unload
  });
  window_stack_push(s_sel_window, true);
}
