#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../ui.h"
#include "../minigames.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_MENU — centralized navigation menu
// ---------------------------------------------------------------------------
static Window  *s_menu_window  = NULL;
static Layer   *s_menu_layer   = NULL;
static uint8_t  s_menu_cursor  = 0;
static bool     s_menu_has_adv = false;
static bool     s_menu_confirm_end = false;

// Menu items (dynamic based on adventure state):
// 0: "New Adventure" or "Games"
// 1: "Stats"
// 2: "Options"
// 3: "End Adventure" (only when adventure active)

static uint8_t menu_item_count(void) {
  return s_menu_has_adv ? 4 : 3;
}

static const char *menu_item_label(uint8_t idx) {
  if (idx == 0) return s_menu_has_adv ? "Games" : "New Adventure";
  if (idx == 1) return "Stats";
  if (idx == 2) return "Options";
  if (idx == 3) return "End Adventure";
  return "";
}

static void menu_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  if (s_menu_confirm_end) {
    // End adventure confirmation
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "ABANDON?",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, h / 2 - 36, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "XP will be forfeit.",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 - 10, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "SELECT: Abandon",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, h - 36, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "BACK: Cancel",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h - 18, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "MENU",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 26, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  uint8_t count = menu_item_count();
  for (uint8_t i = 0; i < count; i++) {
    int16_t row_y = 56 + (int16_t)(i * 22);
    bool sel = (s_menu_cursor == i);
    GColor text_color = ui_draw_menu_row(ctx, row_y, w, 18, sel);

    // "End Adventure" gets red text
    if (i == 3) {
      graphics_context_set_text_color(ctx,
        sel ? PBL_IF_COLOR_ELSE(GColorRed, GColorBlack) : PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    } else {
      graphics_context_set_text_color(ctx, text_color);
    }

    graphics_draw_text(ctx, menu_item_label(i),
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void menu_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_menu_confirm_end && s_menu_cursor > 0) s_menu_cursor--;
  layer_mark_dirty(s_menu_layer);
}

static void menu_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_menu_confirm_end && s_menu_cursor < menu_item_count() - 1) s_menu_cursor++;
  layer_mark_dirty(s_menu_layer);
}

static void menu_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;

  if (s_menu_confirm_end) {
    // Confirmed: clear adventure
    Adventure adv;
    memset(&adv, 0, sizeof(Adventure));
    adventure_save(&adv);
    window_stack_pop(true);  // back to adventure screen (will reload in idle mode)
    return;
  }

  if (s_menu_cursor == 0) {
    // "New Adventure" or "Games"
    if (s_menu_has_adv) {
      minigames_push_selection();
    } else {
      Pet pet;
      pet_load(&pet);
      Adventure adv;
      adventure_init(&adv, &pet);
      adventure_save(&adv);
      window_stack_pop(true);  // back to adventure screen (will reload in active mode)
    }
  } else if (s_menu_cursor == 1) {
    // "Stats"
    screens_push_stats();
  } else if (s_menu_cursor == 2) {
    // "Options"
    screens_push_options();
  } else if (s_menu_cursor == 3) {
    // "End Adventure"
    s_menu_confirm_end = true;
    layer_mark_dirty(s_menu_layer);
  }
}

static void menu_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_menu_confirm_end) {
    s_menu_confirm_end = false;
    layer_mark_dirty(s_menu_layer);
  } else {
    window_stack_pop(true);
  }
}

static void menu_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     menu_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   menu_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, menu_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK,   menu_click_back);
}

static void menu_window_load(Window *window) {
  Adventure adv;
  s_menu_has_adv = adventure_load(&adv) && adv.active;
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_menu_layer = layer_create(bounds);
  layer_set_update_proc(s_menu_layer, menu_layer_update);
  layer_add_child(root, s_menu_layer);
  window_set_click_config_provider(window, menu_click_config);
}

static void menu_window_unload(Window *window) {
  layer_destroy(s_menu_layer); s_menu_layer = NULL;
  window_destroy(s_menu_window); s_menu_window = NULL;
}

void screens_push_menu(void) {
  s_menu_cursor = 0;
  s_menu_confirm_end = false;
  s_menu_window = window_create();
  window_set_background_color(s_menu_window, GColorBlack);
  window_set_window_handlers(s_menu_window, (WindowHandlers) {
    .load   = menu_window_load,
    .unload = menu_window_unload
  });
  window_stack_push(s_menu_window, true);
}
