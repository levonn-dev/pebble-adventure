/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../minigames.h"
#include "minigames_internal.h"
#include "../shared_types.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// CATCH MINI-GAME (DEX)
// ---------------------------------------------------------------------------
#define CATCH_TICK_MS   60
#define CATCH_GOAL      5
#define CATCH_MAX_MISS  3

static Window   *s_ct_window = NULL;
static Layer    *s_ct_layer  = NULL;
static AppTimer *s_ct_timer  = NULL;

static int16_t  s_ct_item_y  = -10;
static bool     s_ct_item_active = false;
static uint8_t  s_ct_caught  = 0;
static uint8_t  s_ct_missed  = 0;
static bool     s_ct_done    = false;
static bool     s_ct_won     = false;
static uint16_t s_ct_dex     = 1;
static int16_t  s_ct_catch_zone_y = 0;
static int16_t  s_ct_catch_zone_h = 0;

static void ct_pop_callback(void *data) {
  window_stack_pop(true);
}

static void ct_spawn_item(void) {
  s_ct_item_y = -10;
  s_ct_item_active = true;
}

static void ct_end_game(bool won) {
  s_ct_done = true;
  s_ct_won = won;
  if (s_ct_timer) { app_timer_cancel(s_ct_timer); s_ct_timer = NULL; }

  if (won) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active) {
      uint8_t seg = adv.current_segment;
      if (seg < adv.num_segments) {
        uint32_t remaining = adv.segment_length[seg] - adv.segment_progress[seg];
        uint32_t reduction = remaining * 5 / 100;
        if (reduction > 0) {
          adv.segment_length[seg] -= reduction;
          if (adv.segment_length[seg] <= adv.segment_progress[seg]) {
            adv.segment_length[seg] = adv.segment_progress[seg] + 1;
          }
        }
        adventure_save(&adv);
      }
    }
  }

  layer_mark_dirty(s_ct_layer);
  s_ct_timer = app_timer_register(2000, ct_pop_callback, NULL);
}

static void ct_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_status_bar(ctx, bounds);

  if (s_ct_done) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(
      s_ct_won ? GColorGreen : GColorRed, GColorWhite));
    graphics_draw_text(ctx, s_ct_won ? "CAUGHT!" : "DROPPED!",
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, h / 2 - 16, w, 28),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_ct_won ? "+5% step bonus" : "No reward",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 + 14, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // Score
  char score_buf[28];
  snprintf(score_buf, sizeof(score_buf), "Catch: %d/%d  Miss: %d",
           (int)s_ct_caught, CATCH_GOAL, (int)s_ct_missed);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, score_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, 24, w - 8, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Catch zone
  s_ct_catch_zone_h = 12 + (int16_t)(s_ct_dex / 15);
  if (s_ct_catch_zone_h > 40) s_ct_catch_zone_h = 40;
  s_ct_catch_zone_y = h - 40 - s_ct_catch_zone_h / 2;

  GRect zone = GRect(w / 4, s_ct_catch_zone_y, w / 2, s_ct_catch_zone_h);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, zone, 3);

  // Falling item
  if (s_ct_item_active) {
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_fill_circle(ctx, GPoint(w / 2, s_ct_item_y), 6);
  }

  // Fox at bottom
  ui_draw_fox(ctx, GPoint(w / 2, h - 16), FOX_IDLE, 0);

  // Hint
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "SELECT to catch!",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 16, w, 14),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void ct_tick(void *ctx) {
  if (s_ct_done) return;

  if (!s_ct_item_active) {
    ct_spawn_item();
  } else {
    s_ct_item_y += 7;

    GRect bounds = layer_get_bounds(s_ct_layer);
    if (s_ct_item_y > bounds.size.h) {
      s_ct_item_active = false;
      s_ct_missed++;
      if (s_ct_missed >= CATCH_MAX_MISS) {
        ct_end_game(false);
        return;
      }
    }
  }

  layer_mark_dirty(s_ct_layer);
  s_ct_timer = app_timer_register(CATCH_TICK_MS, ct_tick, NULL);
}

static void ct_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_ct_done || !s_ct_item_active) return;

  if (s_ct_item_y >= s_ct_catch_zone_y &&
      s_ct_item_y <= s_ct_catch_zone_y + s_ct_catch_zone_h) {
    s_ct_caught++;
    s_ct_item_active = false;
    ui_vibe_short();
    if (s_ct_caught >= CATCH_GOAL) {
      ct_end_game(true);
      return;
    }
  } else {
    s_ct_item_active = false;
    s_ct_missed++;
    if (s_ct_missed >= CATCH_MAX_MISS) {
      ct_end_game(false);
      return;
    }
  }
  layer_mark_dirty(s_ct_layer);
}

static void ct_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, ct_click_select);
}

static void ct_window_load(Window *window) {
  light_enable(true);
  Pet pet; pet_load(&pet);
  s_ct_dex = pet.dex;
  // Pre-compute catch zone so click handler has valid values from the start
  GRect pre_bounds = layer_get_bounds(window_get_root_layer(window));
  s_ct_catch_zone_h = 12 + (int16_t)(s_ct_dex / 15);
  if (s_ct_catch_zone_h > 40) s_ct_catch_zone_h = 40;
  s_ct_catch_zone_y = pre_bounds.size.h - 40 - s_ct_catch_zone_h / 2;
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_ct_layer = layer_create(bounds);
  layer_set_update_proc(s_ct_layer, ct_layer_update);
  layer_add_child(root, s_ct_layer);
  window_set_click_config_provider(window, ct_click_config);
  ct_spawn_item();
  s_ct_timer = app_timer_register(CATCH_TICK_MS, ct_tick, NULL);
}

static void ct_window_unload(Window *window) {
  light_enable(false);
  if (s_ct_timer) { app_timer_cancel(s_ct_timer); s_ct_timer = NULL; }
  layer_destroy(s_ct_layer); s_ct_layer = NULL;
  window_destroy(s_ct_window); s_ct_window = NULL;
}

void minigame_push_catch(void) {
  s_ct_caught = 0;
  s_ct_missed = 0;
  s_ct_item_y = -10;
  s_ct_item_active = false;
  s_ct_done = false;
  s_ct_won = false;

  s_ct_window = window_create();
  window_set_background_color(s_ct_window, GColorBlack);
  window_set_window_handlers(s_ct_window, (WindowHandlers) {
    .load   = ct_window_load,
    .unload = ct_window_unload
  });
  window_stack_push(s_ct_window, true);
}
