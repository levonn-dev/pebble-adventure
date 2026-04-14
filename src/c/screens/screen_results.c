#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_RESULTS
// ---------------------------------------------------------------------------
static Window  *s_res_window     = NULL;
static Layer   *s_res_layer      = NULL;
static uint32_t s_res_xp         = 0;
static uint8_t  s_res_encounters = 0;
static uint8_t  s_res_lvls       = 0;
static Pet      s_res_pet;

static void res_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "ADVENTURE DONE",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 26, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char buf[32];
  graphics_context_set_text_color(ctx, GColorWhite);

  snprintf(buf, sizeof(buf), "XP earned: %lu", (unsigned long)s_res_xp);
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(12, 54, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  snprintf(buf, sizeof(buf), "Encounters: %d", (int)s_res_encounters);
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(12, 72, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (s_res_lvls > 0) {
    snprintf(buf, sizeof(buf), "LEVEL UP x%d!", (int)s_res_lvls);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
    graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, 94, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // XP progress bar for current level -- uses cached s_res_pet from window_load
  snprintf(buf, sizeof(buf), "Lv.%d", (int)s_res_pet.level);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(8, h - 44, 36, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ui_draw_progress_bar(ctx, GRect(48, h - 40, w - 58, 10), s_res_pet.xp, s_res_pet.xp_next_level);

  const char *action = (s_res_lvls > 0) ? "SELECT: Alloc pts" : "SELECT: Continue";
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, action, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 18, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void res_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_res_lvls > 0) {
    screens_push_levelup(&s_res_pet);
    window_stack_pop(false);  // remove results, levelup is on top of adventure
  } else {
    window_stack_pop(true);  // back to adventure screen
  }
}

static void res_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, res_click_select);
}

static void res_window_load(Window *window) {
  // Apply XP and save updated pet
  pet_load(&s_res_pet);
  s_res_lvls = stats_apply_xp(&s_res_pet, s_res_xp);
  pet_save(&s_res_pet);

  // Clear adventure
  Adventure blank; memset(&blank, 0, sizeof(Adventure));
  adventure_save(&blank);

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_res_layer = layer_create(bounds);
  layer_set_update_proc(s_res_layer, res_layer_update);
  layer_add_child(root, s_res_layer);
  window_set_click_config_provider(window, res_click_config);
}

static void res_window_unload(Window *window) {
  layer_destroy(s_res_layer); s_res_layer = NULL;
  window_destroy(s_res_window); s_res_window = NULL;
}

void screens_push_results(uint32_t xp_earned, uint8_t encounters) {
  s_res_xp         = xp_earned;
  s_res_encounters = encounters;
  s_res_lvls       = 0;

  s_res_window = window_create();
  window_set_background_color(s_res_window, GColorBlack);
  window_set_window_handlers(s_res_window, (WindowHandlers) {
    .load   = res_window_load,
    .unload = res_window_unload
  });
  window_stack_push(s_res_window, true);
}
