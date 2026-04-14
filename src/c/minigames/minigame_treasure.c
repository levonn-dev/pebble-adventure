#include "../minigames.h"
#include "minigames_internal.h"
#include "../shared_types.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// TREASURE HUNT MINI-GAME (LUK)
// ---------------------------------------------------------------------------
#define TREASURE_SPOTS 8

typedef enum {
  SPOT_EMPTY = 0,
  SPOT_SMALL = 1,
  SPOT_LARGE = 2,
} SpotType;

static Window  *s_th_window  = NULL;
static Layer   *s_th_layer   = NULL;
static uint8_t  s_th_cursor  = 0;
static bool     s_th_revealed = false;
static SpotType s_th_spots[TREASURE_SPOTS];
static SpotType s_th_pick    = SPOT_EMPTY;

static void th_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "TREASURE HUNT",
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, 24, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // 2x4 grid
  int16_t cell_w = (w - 16) / 4;
  int16_t cell_h = 32;
  int16_t grid_x = 8;
  int16_t grid_y = 46;

  for (uint8_t i = 0; i < TREASURE_SPOTS; i++) {
    uint8_t row = i / 4;
    uint8_t col = i % 4;
    int16_t cx = grid_x + col * cell_w;
    int16_t cy = grid_y + row * (cell_h + 4);
    GRect cell = GRect(cx, cy, cell_w - 2, cell_h);

    if (s_th_revealed) {
      GColor fill;
      const char *label;
      if (s_th_spots[i] == SPOT_LARGE) {
        fill = PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite);
        label = "!!";
      } else if (s_th_spots[i] == SPOT_SMALL) {
        fill = PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite);
        label = "!";
      } else {
        fill = PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack);
        label = "-";
      }
      graphics_context_set_fill_color(ctx, fill);
      graphics_fill_rect(ctx, cell, 3, GCornersAll);

      if (i == s_th_cursor) {
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
        graphics_context_set_stroke_width(ctx, 3);
        graphics_draw_round_rect(ctx, cell, 3);
        graphics_context_set_stroke_width(ctx, 1);
      }

      graphics_context_set_text_color(ctx, GColorBlack);
      graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(cx + 2, cy + 6, cell_w - 6, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      bool sel = (s_th_cursor == i);
      graphics_context_set_fill_color(ctx,
        sel ? PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite)
            : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
      graphics_fill_rect(ctx, cell, 3, GCornersAll);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_round_rect(ctx, cell, 3);

      graphics_context_set_text_color(ctx, sel ? GColorBlack : GColorWhite);
      graphics_draw_text(ctx, "?", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(cx + 2, cy + 6, cell_w - 6, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }

  // Result text
  if (s_th_revealed) {
    const char *result_text;
    if (s_th_pick == SPOT_LARGE) {
      result_text = "BIG FIND! +20%";
    } else if (s_th_pick == SPOT_SMALL) {
      result_text = "Found! +10%";
    } else {
      result_text = "Empty...";
    }
    // Fox reaction to find
    int16_t fox_y = grid_y + 2 * (cell_h + 4) + 16;
    ui_draw_fox(ctx, GPoint(w / 2, fox_y),
                s_th_pick != SPOT_EMPTY ? FOX_HAPPY : FOX_SAD, 0);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, result_text,
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, h - 36, w, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    // Digging fox follows cursor
    int16_t fox_y = grid_y + 2 * (cell_h + 4) + 16;
    int16_t fox_x = grid_x + (s_th_cursor % 4) * cell_w + cell_w / 2;
    ui_draw_fox(ctx, GPoint(fox_x, fox_y), FOX_DIG, (uint8_t)(time(NULL) % 2));
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "UP/DN: move  SEL: dig",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h - 18, w, 14),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void th_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_th_revealed && s_th_cursor > 0) {
    s_th_cursor--;
    layer_mark_dirty(s_th_layer);
  }
}

static void th_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_th_revealed && s_th_cursor < TREASURE_SPOTS - 1) {
    s_th_cursor++;
    layer_mark_dirty(s_th_layer);
  }
}

static void th_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_th_revealed) {
    window_stack_pop(true);
    return;
  }

  s_th_revealed = true;
  s_th_pick = s_th_spots[s_th_cursor];

  int8_t reward;
  if (s_th_pick == SPOT_LARGE) reward = 20;
  else if (s_th_pick == SPOT_SMALL) reward = 10;
  else reward = 0;

  Adventure adv;
  if (adventure_load(&adv) && adv.active) {
    adventure_add_progress_pct(&adv, reward);
    adventure_save(&adv);
  }

  ui_vibe_short();
  layer_mark_dirty(s_th_layer);
}

static void th_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     th_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   th_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, th_click_select);
}

static void th_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_th_layer = layer_create(bounds);
  layer_set_update_proc(s_th_layer, th_layer_update);
  layer_add_child(root, s_th_layer);
  window_set_click_config_provider(window, th_click_config);
}

static void th_window_unload(Window *window) {
  layer_destroy(s_th_layer); s_th_layer = NULL;
  window_destroy(s_th_window); s_th_window = NULL;
}

void minigame_push_treasure(void) {
  s_th_cursor = 0;
  s_th_revealed = false;

  // Generate grid based on LUK
  Pet pet; pet_load(&pet);
  uint8_t total_prizes = 2 + (uint8_t)(pet.luk / 50);
  if (total_prizes > TREASURE_SPOTS) total_prizes = TREASURE_SPOTS;

  uint8_t large_ratio = (uint8_t)(pet.luk / 100);
  uint8_t num_large = total_prizes * large_ratio / 10;
  uint8_t num_small = total_prizes - num_large;

  memset(s_th_spots, 0, sizeof(s_th_spots));

  uint8_t placed = 0;
  for (uint8_t tries = 0; tries < 64 && placed < num_large; tries++) {
    uint8_t pos = (uint8_t)(rand() % TREASURE_SPOTS);
    if (s_th_spots[pos] == SPOT_EMPTY) {
      s_th_spots[pos] = SPOT_LARGE;
      placed++;
    }
  }
  placed = 0;
  for (uint8_t tries = 0; tries < 64 && placed < num_small; tries++) {
    uint8_t pos = (uint8_t)(rand() % TREASURE_SPOTS);
    if (s_th_spots[pos] == SPOT_EMPTY) {
      s_th_spots[pos] = SPOT_SMALL;
      placed++;
    }
  }

  s_th_window = window_create();
  window_set_background_color(s_th_window, GColorBlack);
  window_set_window_handlers(s_th_window, (WindowHandlers) {
    .load   = th_window_load,
    .unload = th_window_unload
  });
  window_stack_push(s_th_window, true);
}
