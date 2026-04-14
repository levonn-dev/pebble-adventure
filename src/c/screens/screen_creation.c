#include "../screens.h"
#include "screens_internal.h"
#include "screen_stat_alloc.h"
#include "../game_state.h"
#include "../stats.h"
#include "../ui.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_CREATION — name entry only (stat allocation is shared screen)
// ---------------------------------------------------------------------------
Window  *s_cr_window = NULL;
static Layer   *s_cr_layer  = NULL;
static uint8_t  s_cr_phase  = 0;           // 0=name, 1=confirm
static char     s_cr_name[12];
static uint8_t  s_cr_cursor = 0;
static Pet      s_cr_pet;

const char *s_stat_names[NUM_STATS] = {
  "STR", "DEX", "AGI", "VIT", "INT", "LUK"
};

// Character cycle order: A-Z a-z 0-9 ! @ # $ % & * - _ + = . , ? ' (space)
static const char s_char_table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
  "0123456789!@#$%&*-_+=.,?' ";
#define CHAR_TABLE_LEN ((int)(sizeof(s_char_table) - 1))

// Binary search state for long-press navigation
static int8_t  s_cr_last_long_dir = 0;
static int16_t s_cr_prev_anchor   = 0;

static int16_t cr_char_index(char c) {
  for (int16_t i = 0; i < CHAR_TABLE_LEN; i++) {
    if (s_char_table[i] == c) return i;
  }
  return 0;
}

static void cr_cycle_char(int8_t dir) {
  char c = s_cr_name[s_cr_cursor];
  if (c == '\0') {
    s_cr_name[s_cr_cursor] = (dir > 0) ? 'A' : s_char_table[CHAR_TABLE_LEN - 2];
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

  if (s_cr_last_long_dir == 0 || dir == s_cr_last_long_dir) {
    s_cr_prev_anchor = cur;
    if (dir > 0) {
      target = cur + (CHAR_TABLE_LEN - 1 - cur) / 2;
    } else {
      target = cur / 2;
    }
  } else {
    int16_t mid = (cur + s_cr_prev_anchor) / 2;
    s_cr_prev_anchor = cur;
    target = mid;
  }

  if (target == cur) target += dir;
  if (target >= CHAR_TABLE_LEN) target = CHAR_TABLE_LEN - 1;
  if (target < 0) target = 0;

  s_cr_last_long_dir = dir;
  s_cr_name[s_cr_cursor] = s_char_table[target];
}

// ---------------------------------------------------------------------------
// Done callback from stat allocation screen
// ---------------------------------------------------------------------------
static void cr_deferred_remove(void *data) {
  Window *w = (Window *)data;
  if (w) window_stack_remove(w, false);
}

static void cr_stat_alloc_done(Pet *pet) {
  pet_save(pet);
  Adventure blank;
  memset(&blank, 0, sizeof(Adventure));
  adventure_save(&blank);
  if (!app_worker_is_running()) app_worker_launch();
  screens_push_adventure();
  // The stat alloc screen will pop itself via window_stack_pop.
  // Remove creation window underneath.
  if (s_cr_window) {
    app_timer_register(50, cr_deferred_remove, s_cr_window);
  }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
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

    // Bracket cursor: "Fo[x]" or "Fox[ ]"
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

  } else {
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
  }
}

// ---------------------------------------------------------------------------
// Click handlers
// ---------------------------------------------------------------------------
static void cr_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) { cr_cycle_char(1); }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) { cr_cycle_char(-1); }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) {
    if (s_cr_name[s_cr_cursor] == '\0') return;
    if (s_cr_cursor < 10) {
      s_cr_cursor++;
    }
  } else if (s_cr_phase == 1) {
    // Confirm name → push stat allocation screen
    pet_init_new(&s_cr_pet, s_cr_name);
    screens_push_stat_alloc("ALLOCATE STATS", &s_cr_pet, cr_stat_alloc_done);
  }
  layer_mark_dirty(s_cr_layer);
}

static void cr_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) {
    uint8_t len = (uint8_t)strlen(s_cr_name);
    if (s_cr_cursor > len && s_cr_cursor > 0) {
      s_cr_cursor--;
    } else if (s_cr_cursor > 0) {
      s_cr_cursor--;
      memset(&s_cr_name[s_cr_cursor], 0, sizeof(s_cr_name) - s_cr_cursor);
    } else if (len > 0) {
      memset(s_cr_name, 0, sizeof(s_cr_name));
    }
  } else if (s_cr_phase == 1) {
    s_cr_phase = 0;
    s_cr_cursor = (uint8_t)strlen(s_cr_name);
  }
  layer_mark_dirty(s_cr_layer);
}

static void cr_long_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_cr_phase == 0) {
    uint8_t len = (uint8_t)strlen(s_cr_name);
    while (len > 0 && s_cr_name[len - 1] == ' ') {
      s_cr_name[--len] = '\0';
    }
    if (len == 0) return;
    s_cr_cursor = len;
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
  window_single_click_subscribe(BUTTON_ID_BACK,   cr_click_back);
  window_long_click_subscribe(BUTTON_ID_UP,     500, cr_long_up, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN,   500, cr_long_down, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, cr_long_select, NULL);
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------
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
  app_timer_register(50, cr_deferred_destroy, s_cr_window);
  s_cr_window = NULL;
}

void screens_push_creation(void) {
  s_cr_phase = 0;
  s_cr_cursor = 3;
  memset(s_cr_name, 0, sizeof(s_cr_name));
  strcpy(s_cr_name, "Fox");

  s_cr_window = window_create();
  window_set_background_color(s_cr_window, GColorBlack);
  window_set_window_handlers(s_cr_window, (WindowHandlers) {
    .load   = cr_window_load,
    .unload = cr_window_unload
  });
  window_stack_push(s_cr_window, true);
}
