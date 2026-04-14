#include "screens.h"
#include "game_state.h"
#include "stats.h"
#include "ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_CREATION
// ---------------------------------------------------------------------------
static Window  *s_cr_window = NULL;
static Layer   *s_cr_layer  = NULL;
static uint8_t  s_cr_phase  = 0;           // 0=name, 1=stats
static char     s_cr_name[12];
static uint8_t  s_cr_cursor = 0;
static Pet      s_cr_pet;
static uint8_t  s_cr_stat_row = 0;         // 0-5 = stat, 6 = Done
static uint16_t s_cr_allocated[NUM_STATS]; // points added this session per stat

static const char *s_stat_names[NUM_STATS] = {
  "STR", "DEX", "AGI", "VIT", "INT", "LUK"
};

static void cr_cycle_char(int8_t dir) {
  char c = s_cr_name[s_cr_cursor];
  if (c == '\0') c = 'A';
  else if (c == ' ') c = (dir > 0) ? 'A' : 'Z';
  else {
    c += dir;
    if (c > 'Z') c = ' ';
    if (c < 'A') c = ' ';
  }
  s_cr_name[s_cr_cursor] = c;
}

static void cr_confirm_name(void) {
  s_cr_phase = 1;
  pet_init_new(&s_cr_pet, s_cr_name);
  memset(s_cr_allocated, 0, sizeof(s_cr_allocated));
  s_cr_stat_row = 0;
}

static void cr_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  if (s_cr_phase == 0) {
    // --- Name entry ---
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "NAME YOUR FOX",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, 26, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    char display[14];
    snprintf(display, sizeof(display), "%s_", s_cr_name);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, display,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, 54, w, 28),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "UP/DN: letter  BACK: del",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, 92, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "SELECT: next / confirm",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, 108, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    // --- Stat allocation ---
    char pts_buf[20];
    snprintf(pts_buf, sizeof(pts_buf), "Points: %d", (int)s_cr_pet.upgrade_points);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, pts_buf,
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, 24, w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    for (uint8_t i = 0; i < NUM_STATS; i++) {
      int16_t row_y = 44 + (int16_t)(i * 17);
      bool sel = (s_cr_stat_row == i);

      if (sel) {
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_rect(ctx, GRect(0, row_y - 1, w, 16), 0, GCornerNone);
      }

      graphics_context_set_text_color(ctx,
        sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorWhite);
      graphics_draw_text(ctx, s_stat_names[i],
        fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
        GRect(4, row_y, 28, 15),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

      char val_buf[6];
      snprintf(val_buf, sizeof(val_buf), "%d", (int)pet_get_stat(&s_cr_pet, i));
      graphics_draw_text(ctx, val_buf,
        fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
        GRect(w - 30, row_y, 28, 15),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

      // Mini bar (stat 1-99 range shown in first tier width)
      int16_t bar_x = 34, bar_w = w - 68;
      uint16_t sv = pet_get_stat(&s_cr_pet, i);
      int16_t fill = (int16_t)((sv > 1 ? sv - 1 : 0) * bar_w / 98);
      if (fill > bar_w) fill = bar_w;
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(bar_x, row_y + 5, bar_w, 6), 1, GCornersAll);
      if (fill > 0) {
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
        graphics_fill_rect(ctx, GRect(bar_x, row_y + 5, fill, 6), 1, GCornersAll);
      }
    }

    // Done row
    bool done_sel = (s_cr_stat_row == NUM_STATS);
    graphics_context_set_text_color(ctx,
      done_sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorLightGray);
    graphics_draw_text(ctx, done_sel ? "> DONE <" : "  DONE  ",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, 44 + NUM_STATS * 17, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void cr_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) { cr_cycle_char(1); }
  else if (s_cr_stat_row > 0) { s_cr_stat_row--; }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) { cr_cycle_char(-1); }
  else if (s_cr_stat_row < NUM_STATS) { s_cr_stat_row++; }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) {
    uint8_t len = (uint8_t)strlen(s_cr_name);
    if (s_cr_cursor < len && s_cr_cursor < 10) {
      s_cr_cursor++;
      if (s_cr_cursor >= len) cr_confirm_name();
    } else {
      cr_confirm_name();
    }
  } else {
    if (s_cr_stat_row < NUM_STATS) {
      uint16_t val = pet_get_stat(&s_cr_pet, s_cr_stat_row);
      if (stats_raise_stat(&val, &s_cr_pet.upgrade_points)) {
        pet_set_stat(&s_cr_pet, s_cr_stat_row, val);
        s_cr_allocated[s_cr_stat_row]++;
      }
    } else {
      // Done — save pet, blank adventure, go to main
      pet_save(&s_cr_pet);
      Adventure blank;
      memset(&blank, 0, sizeof(Adventure));
      adventure_save(&blank);
      window_stack_pop(true);
      screens_push_main();
      if (!app_worker_is_running()) app_worker_launch();
    }
  }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) {
    if (s_cr_cursor > 0) {
      s_cr_cursor--;
      s_cr_name[s_cr_cursor] = '\0';
    }
  } else if (s_cr_stat_row < NUM_STATS && s_cr_allocated[s_cr_stat_row] > 0) {
    uint16_t val = pet_get_stat(&s_cr_pet, s_cr_stat_row);
    if (stats_lower_stat(&val, &s_cr_pet.upgrade_points)) {
      pet_set_stat(&s_cr_pet, s_cr_stat_row, val);
      s_cr_allocated[s_cr_stat_row]--;
    }
  }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,    cr_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,  cr_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, cr_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK,   cr_click_back);
}

static void cr_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_cr_layer = layer_create(bounds);
  layer_set_update_proc(s_cr_layer, cr_layer_update);
  layer_add_child(root, s_cr_layer);
  window_set_click_config_provider(window, cr_click_config);
}

static void cr_window_unload(Window *window) {
  layer_destroy(s_cr_layer);
  s_cr_layer = NULL;
  window_destroy(s_cr_window);
  s_cr_window = NULL;
}

void screens_push_creation(void) {
  s_cr_phase = 0;
  s_cr_cursor = 3;
  memset(s_cr_name, 0, sizeof(s_cr_name));
  strcpy(s_cr_name, "Fox");
  s_cr_stat_row = 0;
  memset(s_cr_allocated, 0, sizeof(s_cr_allocated));

  s_cr_window = window_create();
  window_set_background_color(s_cr_window, GColorBlack);
  window_set_window_handlers(s_cr_window, (WindowHandlers) {
    .load   = cr_window_load,
    .unload = cr_window_unload
  });
  window_stack_push(s_cr_window, true);
}
void screens_push_main(void)                                      { /* Task 10 */ }
void screens_push_adventure(void)                                 { /* Task 11 */ }
void screens_push_results(uint32_t xp, uint8_t enc)               { (void)xp; (void)enc; /* Task 12 */ }
void screens_push_levelup(Pet *pet)                               { (void)pet; /* Task 12 */ }
void screens_on_worker_message(uint16_t t, AppWorkerMessage *msg) { (void)t; (void)msg; /* Task 11 */ }
