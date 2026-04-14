#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_CREATION
// ---------------------------------------------------------------------------
Window  *s_cr_window = NULL;
static Layer   *s_cr_layer  = NULL;
static uint8_t  s_cr_phase  = 0;           // 0=name, 1=confirm, 2=stats
static char     s_cr_name[12];
static uint8_t  s_cr_cursor = 0;
static Pet      s_cr_pet;
static uint8_t  s_cr_stat_row = 0;         // 0-5 = stat, 6 = Done
static uint16_t s_cr_allocated[NUM_STATS]; // points added this session per stat
static bool     s_cr_show_help = false;    // stat help overlay visible

const char *s_stat_names[NUM_STATS] = {
  "STR", "DEX", "AGI", "VIT", "INT", "LUK"
};

static const char *s_stat_help[NUM_STATS] = {
  "Strength\nWater & Mountain\nbiome speed",
  "Dexterity\nForest biome speed\nCatch game timing",
  "Agility\nPlains biome speed\nDodge game speed",
  "Vitality\nStorm biome speed\nEndurance",
  "Intelligence\nCave biome speed\nRiddle hints",
  "Luck\nForest & Cave bonus\nTreasure prizes",
};

// Character cycle order: A-Z a-z 0-9 ! @ # $ % & * - _ + = . , ? ' (space)
static const char s_char_table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
  "0123456789!@#$%&*-_+=.,?' ";
#define CHAR_TABLE_LEN ((int)(sizeof(s_char_table) - 1))  // exclude null terminator

// Binary search state for long-press navigation
static int8_t  s_cr_last_long_dir = 0;   // last long-press direction (+1/-1, 0=none)
static int16_t s_cr_prev_anchor   = 0;   // previous position before last long-press

static int16_t cr_char_index(char c) {
  for (int16_t i = 0; i < CHAR_TABLE_LEN; i++) {
    if (s_char_table[i] == c) return i;
  }
  return 0;  // default to 'A'
}

static void cr_cycle_char(int8_t dir) {
  char c = s_cr_name[s_cr_cursor];
  if (c == '\0') {
    // First press on empty slot: show 'A' (up) or 'z' (down), don't advance past it
    s_cr_name[s_cr_cursor] = (dir > 0) ? 'A' : s_char_table[CHAR_TABLE_LEN - 2]; // last before space
    s_cr_last_long_dir = 0;
    return;
  }
  int16_t idx = cr_char_index(c);
  idx += dir;
  if (idx >= CHAR_TABLE_LEN) idx = 0;
  if (idx < 0) idx = CHAR_TABLE_LEN - 1;
  s_cr_name[s_cr_cursor] = s_char_table[idx];
  s_cr_last_long_dir = 0;
}

static void cr_long_press_char(int8_t dir) {
  char c = s_cr_name[s_cr_cursor];
  if (c == '\0') c = 'A';
  int16_t cur = cr_char_index(c);
  int16_t target;

  if (s_cr_last_long_dir == 0) {
    // First long press: jump halfway from current to end in that direction
    s_cr_prev_anchor = cur;
    if (dir > 0) {
      target = cur + (CHAR_TABLE_LEN - 1 - cur) / 2;
    } else {
      target = cur / 2;
    }
  } else if (dir == s_cr_last_long_dir) {
    // Same direction: jump halfway from current to end in that direction
    s_cr_prev_anchor = cur;
    if (dir > 0) {
      target = cur + (CHAR_TABLE_LEN - 1 - cur) / 2;
    } else {
      target = cur / 2;
    }
  } else {
    // Opposite direction: jump halfway between current and previous anchor
    int16_t mid = (cur + s_cr_prev_anchor) / 2;
    s_cr_prev_anchor = cur;
    target = mid;
  }

  if (target == cur) target += dir;  // ensure movement
  if (target >= CHAR_TABLE_LEN) target = CHAR_TABLE_LEN - 1;
  if (target < 0) target = 0;

  s_cr_last_long_dir = dir;
  s_cr_name[s_cr_cursor] = s_char_table[target];
}

static void cr_accept_name(void) {
  s_cr_phase = 2;
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
    graphics_draw_text(ctx, "NAME YOUR PET",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, 26, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    // Show name with bracket cursor: "Fo[x]" or "Fox[ ]"
    char display[20];
    uint8_t len = (uint8_t)strlen(s_cr_name);
    char before[12] = {0};
    if (s_cr_cursor > 0) memcpy(before, s_cr_name, s_cr_cursor);
    char cur_ch = (s_cr_cursor < len) ? s_cr_name[s_cr_cursor] : ' ';
    snprintf(display, sizeof(display), "%s[%c]", before, cur_ch);
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, display,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, 54, w, 28),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "UP/DN: letter  HOLD: jump",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, 92, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "SEL: next  BACK: del",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, 108, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "HOLD SEL: confirm",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, 124, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  } else if (s_cr_phase == 1) {
    // --- Name confirmation ---
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "CONFIRM NAME?",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, 26, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, s_cr_name,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, 60, w, 28),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
    graphics_draw_text(ctx, "SELECT: Confirm",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(0, 100, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "BACK: Edit name",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, 118, w, 16),
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

    // Stat help overlay
    if (s_cr_show_help && s_cr_stat_row < NUM_STATS) {
      GRect bounds = layer_get_bounds(layer);
      int16_t h = bounds.size.h;
      GRect help_rect = GRect(8, h / 2 - 30, w - 16, 60);
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
      graphics_fill_rect(ctx, help_rect, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
      graphics_draw_round_rect(ctx, help_rect, 4);

      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, s_stat_help[s_cr_stat_row],
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(12, h / 2 - 26, w - 24, 52),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }
}

static void cr_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_show_help) { s_cr_show_help = false; layer_mark_dirty(s_cr_layer); return; }
  if (s_cr_phase == 0) { cr_cycle_char(1); }
  else if (s_cr_phase == 2 && s_cr_stat_row > 0) { s_cr_stat_row--; }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_show_help) { s_cr_show_help = false; layer_mark_dirty(s_cr_layer); return; }
  if (s_cr_phase == 0) { cr_cycle_char(-1); }
  else if (s_cr_phase == 2 && s_cr_stat_row < NUM_STATS) { s_cr_stat_row++; }
  layer_mark_dirty(s_cr_layer);
}

static void cr_deferred_remove(void *data) {
  Window *w = (Window *)data;
  if (w) window_stack_remove(w, false);
}

static void cr_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_show_help) { s_cr_show_help = false; layer_mark_dirty(s_cr_layer); return; }
  if (s_cr_phase == 0) {
    // Select advances cursor only if current position has a character
    if (s_cr_name[s_cr_cursor] == '\0') return;  // must type something first
    if (s_cr_cursor < 10) {
      s_cr_cursor++;
    }
  } else if (s_cr_phase == 1) {
    // Confirm name → go to stat allocation
    cr_accept_name();
  } else {
    if (s_cr_stat_row < NUM_STATS) {
      uint16_t val = pet_get_stat(&s_cr_pet, s_cr_stat_row);
      if (stats_raise_stat(&val, &s_cr_pet.upgrade_points)) {
        pet_set_stat(&s_cr_pet, s_cr_stat_row, val);
        s_cr_allocated[s_cr_stat_row]++;
      }
    } else {
      // Done -- save pet, blank adventure, go to main
      pet_save(&s_cr_pet);
      Adventure blank;
      memset(&blank, 0, sizeof(Adventure));
      adventure_save(&blank);
      if (!app_worker_is_running()) app_worker_launch();
      // Push main on top, then schedule creation removal after click handler exits
      screens_push_main();
      app_timer_register(50, cr_deferred_remove, s_cr_window);
    }
  }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_show_help) { s_cr_show_help = false; layer_mark_dirty(s_cr_layer); return; }
  if (s_cr_phase == 0) {
    uint8_t len = (uint8_t)strlen(s_cr_name);
    if (s_cr_cursor > len && s_cr_cursor > 0) {
      // Cursor is past the end (after Select advance) — just move back
      s_cr_cursor--;
    } else if (s_cr_cursor > 0) {
      // Delete character behind cursor
      s_cr_cursor--;
      memset(&s_cr_name[s_cr_cursor], 0, sizeof(s_cr_name) - s_cr_cursor);
    } else if (len > 0) {
      // At position 0 with a character — clear it
      memset(s_cr_name, 0, sizeof(s_cr_name));
    }
  } else if (s_cr_phase == 1) {
    // Back from confirm → return to name entry at end of name
    s_cr_phase = 0;
    s_cr_cursor = (uint8_t)strlen(s_cr_name);
  } else if (s_cr_phase == 2 && s_cr_stat_row < NUM_STATS && s_cr_allocated[s_cr_stat_row] > 0) {
    uint16_t val = pet_get_stat(&s_cr_pet, s_cr_stat_row);
    if (stats_lower_stat(&val, &s_cr_pet.upgrade_points)) {
      pet_set_stat(&s_cr_pet, s_cr_stat_row, val);
      s_cr_allocated[s_cr_stat_row]--;
    }
  }
  layer_mark_dirty(s_cr_layer);
}

static void cr_long_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 2 && s_cr_stat_row < NUM_STATS) {
    // Show stat help overlay
    s_cr_show_help = !s_cr_show_help;
    layer_mark_dirty(s_cr_layer);
    return;
  }
  if (s_cr_phase == 0) {
    // Trim trailing spaces
    uint8_t len = (uint8_t)strlen(s_cr_name);
    while (len > 0 && s_cr_name[len - 1] == ' ') {
      s_cr_name[--len] = '\0';
    }
    if (len == 0) return;  // require at least 1 non-space character
    s_cr_cursor = len;
    // Long-press Select → show name confirmation
    s_cr_phase = 1;
    layer_mark_dirty(s_cr_layer);
  }
}

static void cr_long_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) { cr_long_press_char(1); layer_mark_dirty(s_cr_layer); }
}

static void cr_long_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) { cr_long_press_char(-1); layer_mark_dirty(s_cr_layer); }
}

static void cr_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,    cr_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,  cr_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, cr_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK, cr_click_back);
  window_long_click_subscribe(BUTTON_ID_UP,     500, cr_long_up, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN,   500, cr_long_down, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, cr_long_select, NULL);
}

static void cr_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_cr_layer = layer_create(bounds);
  layer_set_update_proc(s_cr_layer, cr_layer_update);
  layer_add_child(root, s_cr_layer);
  window_set_click_config_provider(window, cr_click_config);
}

static void cr_deferred_destroy(void *data) {
  Window *w = (Window *)data;
  if (w) window_destroy(w);
}

static void cr_window_unload(Window *window) {
  layer_destroy(s_cr_layer);
  s_cr_layer = NULL;
  // Defer window_destroy -- may be called during a click handler
  app_timer_register(50, cr_deferred_destroy, s_cr_window);
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
