#include "screen_stat_alloc.h"
#include "screens_internal.h"
#include "../screens.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// Generic stat allocation screen
// ---------------------------------------------------------------------------
static Window  *s_sa_window  = NULL;
static Layer   *s_sa_layer   = NULL;
static Pet      s_sa_pet;
static uint8_t  s_sa_row     = 0;       // 0-5 = stat, 6 = Done
static uint16_t s_sa_alloc[NUM_STATS];  // points allocated this session
static bool     s_sa_show_help = false;
static const char *s_sa_title = NULL;
static StatAllocDoneCallback s_sa_done_cb = NULL;

static const char *s_sa_stat_help[NUM_STATS] = {
  "Strength\nRaw physical power\nPush through obstacles",
  "Dexterity\nPrecision and timing\nHand-eye coordination",
  "Agility\nSpeed and reflexes\nQuick reactions",
  "Vitality\nEndurance and grit\nWeather harsh conditions",
  "Intelligence\nWit and knowledge\nSolve puzzles faster",
  "Luck\nFortune favors you\nBetter rewards found",
};

static void sa_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  // Layout: compute row height to fit the screen
  // Need: title(18) + points(14) + 6 stats + Done + hint(14) + margins
  int16_t top_y = 22;        // after time
  int16_t hint_h = 14;
  int16_t avail = h - top_y - hint_h - 4;  // space for title+points+stats+done
  int16_t header_h = 32;     // title + points
  int16_t stat_area = avail - header_h;
  int16_t row_h = stat_area / (NUM_STATS + 1);  // +1 for Done row
  if (row_h > 18) row_h = 18;
  if (row_h < 12) row_h = 12;
  int16_t stats_y = top_y + header_h;

  // Title + points on one line to save space on small screens
  char title_buf[32];
  snprintf(title_buf, sizeof(title_buf), "%s  Pts:%d",
           s_sa_title ? s_sa_title : "STATS", (int)s_sa_pet.upgrade_points);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  graphics_draw_text(ctx, title_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, top_y, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Stat rows
  for (uint8_t i = 0; i < NUM_STATS; i++) {
    int16_t row_y = stats_y + (int16_t)(i * row_h);
    bool sel = (s_sa_row == i);
    if (sel) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(0, row_y, w, row_h), 0, GCornerNone);
    }

    uint16_t val = pet_get_stat(&s_sa_pet, i);
    char sbuf[24];
    if (s_sa_alloc[i] > 0) {
      snprintf(sbuf, sizeof(sbuf), "%s:%d(+%d)",
               s_stat_names[i], (int)val, (int)s_sa_alloc[i]);
    } else {
      snprintf(sbuf, sizeof(sbuf), "%s:%d", s_stat_names[i], (int)val);
    }
    graphics_context_set_text_color(ctx,
      sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorWhite);
    graphics_draw_text(ctx, sbuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(4, row_y + 1, w - 8, row_h - 2),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Done row
  int16_t done_y = stats_y + (int16_t)(NUM_STATS * row_h);
  bool done_sel = (s_sa_row == NUM_STATS);
  graphics_context_set_text_color(ctx,
    done_sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorLightGray);
  graphics_draw_text(ctx, done_sel ? "> DONE <" : "  DONE  ",
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, done_y + 1, w, row_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Hint at bottom
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "HOLD SEL: stat info",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - hint_h - 2, w, hint_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Help overlay
  if (s_sa_show_help && s_sa_row < NUM_STATS) {
    GRect help_rect = GRect(8, h / 2 - 30, w - 16, 60);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
    graphics_fill_rect(ctx, help_rect, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_round_rect(ctx, help_rect, 4);

    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_sa_stat_help[s_sa_row],
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(12, h / 2 - 26, w - 24, 52),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void sa_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sa_show_help) { s_sa_show_help = false; layer_mark_dirty(s_sa_layer); return; }
  if (s_sa_row > 0) s_sa_row--;
  layer_mark_dirty(s_sa_layer);
}

static void sa_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sa_show_help) { s_sa_show_help = false; layer_mark_dirty(s_sa_layer); return; }
  if (s_sa_row < NUM_STATS) s_sa_row++;
  layer_mark_dirty(s_sa_layer);
}

static void sa_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sa_show_help) { s_sa_show_help = false; layer_mark_dirty(s_sa_layer); return; }
  if (s_sa_row < NUM_STATS) {
    uint16_t val = pet_get_stat(&s_sa_pet, s_sa_row);
    if (stats_raise_stat(&val, &s_sa_pet.upgrade_points)) {
      pet_set_stat(&s_sa_pet, s_sa_row, val);
      s_sa_alloc[s_sa_row]++;
    }
  } else {
    // Done — invoke callback with the modified pet
    if (s_sa_done_cb) s_sa_done_cb(&s_sa_pet);
  }
  layer_mark_dirty(s_sa_layer);
}

static void sa_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sa_show_help) { s_sa_show_help = false; layer_mark_dirty(s_sa_layer); return; }
  if (s_sa_row < NUM_STATS && s_sa_alloc[s_sa_row] > 0) {
    uint16_t val = pet_get_stat(&s_sa_pet, s_sa_row);
    if (stats_lower_stat(&val, &s_sa_pet.upgrade_points)) {
      pet_set_stat(&s_sa_pet, s_sa_row, val);
      s_sa_alloc[s_sa_row]--;
    }
  }
  layer_mark_dirty(s_sa_layer);
}

static void sa_long_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sa_row < NUM_STATS) {
    s_sa_show_help = !s_sa_show_help;
    layer_mark_dirty(s_sa_layer);
  }
}

static void sa_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     sa_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   sa_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, sa_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK,   sa_click_back);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, sa_long_select, NULL);
}

static void sa_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_sa_layer = layer_create(bounds);
  layer_set_update_proc(s_sa_layer, sa_layer_update);
  layer_add_child(root, s_sa_layer);
  window_set_click_config_provider(window, sa_click_config);
}

static void sa_window_unload(Window *window) {
  layer_destroy(s_sa_layer); s_sa_layer = NULL;
  window_destroy(s_sa_window); s_sa_window = NULL;
}

void screens_push_stat_alloc(const char *title, Pet *pet, StatAllocDoneCallback done_cb) {
  s_sa_pet = *pet;
  s_sa_row = 0;
  s_sa_show_help = false;
  s_sa_title = title;
  s_sa_done_cb = done_cb;
  memset(s_sa_alloc, 0, sizeof(s_sa_alloc));

  s_sa_window = window_create();
  window_set_background_color(s_sa_window, GColorBlack);
  window_set_window_handlers(s_sa_window, (WindowHandlers) {
    .load   = sa_window_load,
    .unload = sa_window_unload
  });
  window_stack_push(s_sa_window, true);
}
