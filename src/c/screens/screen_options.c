/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../events.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_OPTIONS
// ---------------------------------------------------------------------------
static Window  *s_opt_window  = NULL;
static Layer   *s_opt_layer   = NULL;
static uint8_t  s_opt_cursor  = 0;
static bool     s_opt_confirm = false;
static Pet      s_opt_pet;

#define OPT_VIBRATION       0
#define OPT_AUTO_ADVENTURE  1
#define OPT_DELETE          2
#ifdef DEBUG_MODE
#define OPT_SKIP_SEG   3
#define OPT_ADD_STEPS  4
#define OPT_ENCOUNTER  5
#define OPT_COUNT      6
#else
#define OPT_COUNT      3
#endif

static void opt_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_status_bar(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "OPTIONS",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 26, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (s_opt_confirm) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "DELETE FOX?",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, h / 2 - 28, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    char name_buf[20];
    snprintf(name_buf, sizeof(name_buf), "\"%s\" Lv.%d", s_opt_pet.name, (int)s_opt_pet.level);
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

    // Auto-Adventure toggle
    int16_t row_auto_y = row0_y + 24;
    GColor c_auto = ui_draw_menu_row(ctx, row_auto_y, w, 18, s_opt_cursor == OPT_AUTO_ADVENTURE);
    char auto_buf[24];
    snprintf(auto_buf, sizeof(auto_buf), "Auto-Adv: %s", ui_auto_adventure_enabled() ? "ON" : "OFF");
    graphics_context_set_text_color(ctx, c_auto);
    graphics_draw_text(ctx, auto_buf,
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row_auto_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Delete pet
    int16_t row1_y = row_auto_y + 24;
    ui_draw_menu_row(ctx, row1_y, w, 18, s_opt_cursor == OPT_DELETE);
    graphics_context_set_text_color(ctx,
      s_opt_cursor == OPT_DELETE ? PBL_IF_COLOR_ELSE(GColorRed, GColorBlack) : PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_draw_text(ctx, "Delete Pet",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row1_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

#ifdef DEBUG_MODE
    int16_t row2_y = row1_y + 24;
    GColor c2 = ui_draw_menu_row(ctx, row2_y, w, 18, s_opt_cursor == OPT_SKIP_SEG);
    graphics_context_set_text_color(ctx, c2);
    graphics_draw_text(ctx, "[DBG] Skip Seg",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row2_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    int16_t row3_y = row2_y + 24;
    GColor c3 = ui_draw_menu_row(ctx, row3_y, w, 18, s_opt_cursor == OPT_ADD_STEPS);
    graphics_context_set_text_color(ctx, c3);
    graphics_draw_text(ctx, "[DBG] +100 Steps",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row3_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    int16_t row4_y = row3_y + 24;
    GColor c4 = ui_draw_menu_row(ctx, row4_y, w, 18, s_opt_cursor == OPT_ENCOUNTER);
    graphics_context_set_text_color(ctx, c4);
    graphics_draw_text(ctx, "[DBG] Encounter",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row4_y, w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
#endif

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "BACK: Return",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h - 18, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void opt_deferred_remove(void *data) {
  Window *w = (Window *)data;
  if (w) window_stack_remove(w, false);
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
    // Remove options window (adventure screen below will be handled by creation flow)
    app_timer_register(50, opt_deferred_remove, s_opt_window);
    return;
  }

  if (s_opt_cursor == OPT_VIBRATION) {
    ui_set_vibration(!ui_vibration_enabled());
    layer_mark_dirty(s_opt_layer);
  } else if (s_opt_cursor == OPT_AUTO_ADVENTURE) {
    ui_set_auto_adventure(!ui_auto_adventure_enabled());
    layer_mark_dirty(s_opt_layer);
  } else if (s_opt_cursor == OPT_DELETE) {
    s_opt_confirm = true;
    layer_mark_dirty(s_opt_layer);
  }
#ifdef DEBUG_MODE
  else if (s_opt_cursor == OPT_SKIP_SEG) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active && adv.current_segment < adv.num_segments) {
      adv.segment_progress[adv.current_segment] = adv.segment_length[adv.current_segment];
      uint8_t diff = biome_get_config((BiomeType)adv.segments[adv.current_segment])->difficulty;
      adv.total_xp_earned += (uint32_t)(adv.current_segment * 50) + (uint32_t)(diff * 30);
      adv.current_segment++;
      if (adv.current_segment >= adv.num_segments) {
        adv.active = false;
      }
      adventure_save(&adv);
    }
    layer_mark_dirty(s_opt_layer);
  } else if (s_opt_cursor == OPT_ADD_STEPS) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active) {
      adventure_apply_steps(&adv, 100);
      adventure_save(&adv);
    }
    layer_mark_dirty(s_opt_layer);
  } else if (s_opt_cursor == OPT_ENCOUNTER) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active) {
      uint8_t enc_id = (uint8_t)(rand() % NUM_ENCOUNTERS);
      Pet pet;
      pet_load(&pet);
      EncounterResult result = encounter_resolve(enc_id, &pet);
      encounter_apply(&result, &adv);
      adv.encounters_total++;
      adventure_save(&adv);
      char detail[20];
      encounter_format_detail(&result, detail, sizeof(detail));
      adv_queue_popup(result.encounter_name, detail, result.won);
      ui_vibe_short();
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
  pet_load(&s_opt_pet);
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

void screens_push_options(void) {
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
