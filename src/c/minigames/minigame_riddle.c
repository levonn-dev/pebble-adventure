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
#include "riddles.h"
#include <pebble.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// RIDDLE MINI-GAME (INT)
// ---------------------------------------------------------------------------
static Window   *s_rd_window  = NULL;
static Layer    *s_rd_layer   = NULL;
static AppTimer *s_rd_timer   = NULL;
static uint8_t   s_rd_cursor  = 0;
static uint8_t  s_rd_riddle_idx = 0;
static bool     s_rd_done    = false;
static bool     s_rd_won     = false;
static bool     s_rd_eliminated[RIDDLE_ANSWERS];
static uint16_t s_rd_intel   = 1;

static void rd_pop_callback(void *data) {
  window_stack_pop(true);
}

static void rd_advance_cursor(int8_t dir) {
  for (int tries = 0; tries < RIDDLE_ANSWERS; tries++) {
    s_rd_cursor = (uint8_t)((s_rd_cursor + dir + RIDDLE_ANSWERS) % RIDDLE_ANSWERS);
    if (!s_rd_eliminated[s_rd_cursor]) return;
  }
}

static void rd_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;
  const Riddle *r = riddle_get(s_rd_riddle_idx);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_status_bar(ctx, bounds);

  if (s_rd_done) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(
      s_rd_won ? GColorGreen : GColorRed, GColorWhite));
    graphics_draw_text(ctx, s_rd_won ? "CORRECT!" : "WRONG!",
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, h / 2 - 24, w, 28),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    char ans_buf[24];
    snprintf(ans_buf, sizeof(ans_buf), "Answer: %s", r->answers[r->correct]);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, ans_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(4, h / 2 + 6, w - 8, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, s_rd_won ? "+20% progress" : "No reward",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 + 24, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // Question
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, r->question,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(4, 24, w - 8, 36),
    GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  // Answers
  uint8_t drawn = 0;
  for (uint8_t i = 0; i < RIDDLE_ANSWERS; i++) {
    if (s_rd_eliminated[i]) continue;
    int16_t row_y = 66 + (int16_t)(drawn * 22);
    bool sel = (s_rd_cursor == i);

    GColor text_color = ui_draw_menu_row(ctx, row_y, w, 18, sel);

    char label[20];
    snprintf(label, sizeof(label), "%c) %s", 'A' + i, r->answers[i]);
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx, label,
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    drawn++;
  }
}

static void rd_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_rd_done) { rd_advance_cursor(-1); layer_mark_dirty(s_rd_layer); }
}

static void rd_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_rd_done) { rd_advance_cursor(1); layer_mark_dirty(s_rd_layer); }
}

static void rd_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_rd_done) return;

  const Riddle *riddle = riddle_get(s_rd_riddle_idx);
  s_rd_done = true;
  s_rd_won = (s_rd_cursor == riddle->correct);

  if (s_rd_won) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active) {
      adventure_add_progress_pct(&adv, 20);
      adventure_save(&adv);
    }
  }

  layer_mark_dirty(s_rd_layer);
  s_rd_timer = app_timer_register(3000, rd_pop_callback, NULL);
}

static void rd_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     rd_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   rd_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, rd_click_select);
}

static void rd_window_load(Window *window) {
  light_enable(true);
  Pet pet; pet_load(&pet);
  s_rd_intel = pet.intel;

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_rd_layer = layer_create(bounds);
  layer_set_update_proc(s_rd_layer, rd_layer_update);
  layer_add_child(root, s_rd_layer);
  window_set_click_config_provider(window, rd_click_config);
}

static void rd_window_unload(Window *window) {
  light_enable(false);
  if (s_rd_timer) { app_timer_cancel(s_rd_timer); s_rd_timer = NULL; }
  layer_destroy(s_rd_layer); s_rd_layer = NULL;
  window_destroy(s_rd_window); s_rd_window = NULL;
}

void minigame_push_riddle(void) {
  s_rd_riddle_idx = riddle_random_index();
  s_rd_cursor = 0;
  s_rd_done = false;
  s_rd_won = false;
  memset(s_rd_eliminated, 0, sizeof(s_rd_eliminated));

  // INT eliminates wrong answers: min(INT/100, 2)
  Pet pet; pet_load(&pet);
  uint8_t hints = (uint8_t)(pet.intel / 100);
  if (hints > 2) hints = 2;

  const Riddle *r = riddle_get(s_rd_riddle_idx);
  uint8_t eliminated = 0;
  for (uint8_t attempts = 0; attempts < 20 && eliminated < hints; attempts++) {
    uint8_t idx = (uint8_t)(rand() % RIDDLE_ANSWERS);
    if (idx != r->correct && !s_rd_eliminated[idx]) {
      s_rd_eliminated[idx] = true;
      eliminated++;
    }
  }

  // Move cursor to first non-eliminated answer
  while (s_rd_eliminated[s_rd_cursor] && s_rd_cursor < RIDDLE_ANSWERS) s_rd_cursor++;

  s_rd_window = window_create();
  window_set_background_color(s_rd_window, GColorBlack);
  window_set_window_handlers(s_rd_window, (WindowHandlers) {
    .load   = rd_window_load,
    .unload = rd_window_unload
  });
  window_stack_push(s_rd_window, true);
}
