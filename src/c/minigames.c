#include "minigames.h"
#include "shared_types.h"
#include "game_state.h"
#include "stats.h"
#include "ui.h"
#include <pebble.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// MINI-GAME SELECTION
// ---------------------------------------------------------------------------
static Window *s_sel_window = NULL;
static Layer  *s_sel_layer  = NULL;
static uint8_t s_sel_cursor = 0;

static const char *s_game_names[4] = {
  "Dodge (AGI)", "Catch (DEX)", "Riddle (INT)", "Treasure (LUK)"
};

static void minigame_push_dodge(void);
static void minigame_push_catch(void);
static void minigame_push_riddle(void);
static void minigame_push_treasure(void);

static void sel_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "MINI-GAMES",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 26, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  for (uint8_t i = 0; i < 4; i++) {
    int16_t row_y = 56 + (int16_t)(i * 22);
    bool sel = (s_sel_cursor == i);

    if (sel) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(0, row_y - 1, w, 20), 0, GCornerNone);
    }

    graphics_context_set_text_color(ctx,
      sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorWhite);
    graphics_draw_text(ctx, s_game_names[i],
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(8, row_y, w - 16, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void sel_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sel_cursor > 0) s_sel_cursor--;
  layer_mark_dirty(s_sel_layer);
}

static void sel_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_sel_cursor < 3) s_sel_cursor++;
  layer_mark_dirty(s_sel_layer);
}

static void sel_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  uint8_t choice = s_sel_cursor;
  window_stack_pop(false);
  switch (choice) {
    case 0: minigame_push_dodge();    break;
    case 1: minigame_push_catch();    break;
    case 2: minigame_push_riddle();   break;
    case 3: minigame_push_treasure(); break;
  }
}

static void sel_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,     sel_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,   sel_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, sel_click_select);
}

static void sel_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_sel_layer = layer_create(bounds);
  layer_set_update_proc(s_sel_layer, sel_layer_update);
  layer_add_child(root, s_sel_layer);
  window_set_click_config_provider(window, sel_click_config);
}

static void sel_window_unload(Window *window) {
  layer_destroy(s_sel_layer); s_sel_layer = NULL;
  window_destroy(s_sel_window); s_sel_window = NULL;
}

void minigames_push_selection(void) {
  s_sel_cursor = 0;
  s_sel_window = window_create();
  window_set_background_color(s_sel_window, GColorBlack);
  window_set_window_handlers(s_sel_window, (WindowHandlers) {
    .load   = sel_window_load,
    .unload = sel_window_unload
  });
  window_stack_push(s_sel_window, true);
}

// ---------------------------------------------------------------------------
// DODGE MINI-GAME (AGI)
// ---------------------------------------------------------------------------
#define DODGE_MAX_ROCKS 8
#define DODGE_TICKS     150
#define DODGE_TICK_MS   100

static Window   *s_dg_window = NULL;
static Layer    *s_dg_layer  = NULL;
static AppTimer *s_dg_timer  = NULL;

static uint8_t  s_dg_lane    = 1;
static uint8_t  s_dg_lives   = 3;
static uint16_t s_dg_ticks   = 0;
static bool     s_dg_done    = false;
static bool     s_dg_won     = false;
static uint16_t s_dg_agi     = 1;

static struct {
  int16_t y;
  uint8_t lane;
  bool    active;
} s_dg_rocks[DODGE_MAX_ROCKS];

static int16_t dg_lane_x(uint8_t lane, int16_t w) {
  return (int16_t)(w * (lane * 2 + 1) / 6);
}

static void dg_spawn_rock(int16_t w) {
  for (int i = 0; i < DODGE_MAX_ROCKS; i++) {
    if (!s_dg_rocks[i].active) {
      s_dg_rocks[i].active = true;
      s_dg_rocks[i].lane = (uint8_t)(rand() % 3);
      s_dg_rocks[i].y = -8;
      return;
    }
  }
}

static void dg_pop_callback(void *data) {
  window_stack_pop(true);
}

static void dg_end_game(bool won) {
  s_dg_done = true;
  s_dg_won = won;
  if (s_dg_timer) { app_timer_cancel(s_dg_timer); s_dg_timer = NULL; }

  if (won) {
    Adventure adv;
    if (adventure_load(&adv) && adv.active) {
      adventure_add_progress_pct(&adv, 10);
      adventure_save(&adv);
    }
  }

  layer_mark_dirty(s_dg_layer);
  s_dg_timer = app_timer_register(2000, dg_pop_callback, NULL);
}

static void dg_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  if (s_dg_done) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(
      s_dg_won ? GColorGreen : GColorRed, GColorWhite));
    graphics_draw_text(ctx, s_dg_won ? "DODGED!" : "HIT!",
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, h / 2 - 16, w, 28),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_dg_won ? "+10% progress" : "No reward",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 + 14, w, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // Timer bar
  uint16_t remaining = DODGE_TICKS - s_dg_ticks;
  ui_draw_progress_bar(ctx, GRect(8, 24, w - 16, 6), (uint32_t)remaining, DODGE_TICKS);

  // Lives
  char lives_buf[12];
  snprintf(lives_buf, sizeof(lives_buf), "Lives: %d", (int)s_dg_lives);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, lives_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, 32, w - 8, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Lane separators
  int16_t ground_y = h - 20;
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 1; i < 3; i++) {
    int16_t lx = (int16_t)(w * i / 3);
    graphics_draw_line(ctx, GPoint(lx, 50), GPoint(lx, ground_y));
  }

  // Fox
  int16_t fox_x = dg_lane_x(s_dg_lane, w);
  ui_draw_fox_placeholder(ctx, GPoint(fox_x, ground_y - 12), (uint8_t)(s_dg_ticks / 3));

  // Rocks
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  for (int i = 0; i < DODGE_MAX_ROCKS; i++) {
    if (s_dg_rocks[i].active) {
      int16_t rx = dg_lane_x(s_dg_rocks[i].lane, w);
      graphics_fill_circle(ctx, GPoint(rx, s_dg_rocks[i].y), 5);
    }
  }
}

static void dg_tick(void *ctx) {
  s_dg_ticks++;
  if (s_dg_ticks >= DODGE_TICKS) {
    dg_end_game(true);
    return;
  }

  // AGI reduces spawn rate: base every 8 ticks, +1 per 50 AGI
  uint16_t spawn_interval = 8 + s_dg_agi / 50;
  if (s_dg_ticks % spawn_interval == 0) {
    GRect bounds = layer_get_bounds(s_dg_layer);
    dg_spawn_rock(bounds.size.w);
  }

  // Move rocks down
  int16_t ground_y = layer_get_bounds(s_dg_layer).size.h - 20;
  int16_t rock_speed = 6 - (int16_t)(s_dg_agi / 200);
  if (rock_speed < 3) rock_speed = 3;

  for (int i = 0; i < DODGE_MAX_ROCKS; i++) {
    if (!s_dg_rocks[i].active) continue;
    s_dg_rocks[i].y += rock_speed;

    if (s_dg_rocks[i].lane == s_dg_lane &&
        s_dg_rocks[i].y >= ground_y - 20 && s_dg_rocks[i].y <= ground_y) {
      s_dg_rocks[i].active = false;
      s_dg_lives--;
      vibes_short_pulse();
      if (s_dg_lives == 0) {
        dg_end_game(false);
        return;
      }
    }

    if (s_dg_rocks[i].y > ground_y + 10) {
      s_dg_rocks[i].active = false;
    }
  }

  layer_mark_dirty(s_dg_layer);
  s_dg_timer = app_timer_register(DODGE_TICK_MS, dg_tick, NULL);
}

static void dg_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_dg_done && s_dg_lane > 0) { s_dg_lane--; layer_mark_dirty(s_dg_layer); }
}

static void dg_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (!s_dg_done && s_dg_lane < 2) { s_dg_lane++; layer_mark_dirty(s_dg_layer); }
}

static void dg_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,   dg_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, dg_click_down);
}

static void dg_window_load(Window *window) {
  Pet pet; pet_load(&pet);
  s_dg_agi = pet.agi;
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_dg_layer = layer_create(bounds);
  layer_set_update_proc(s_dg_layer, dg_layer_update);
  layer_add_child(root, s_dg_layer);
  window_set_click_config_provider(window, dg_click_config);
  s_dg_timer = app_timer_register(DODGE_TICK_MS, dg_tick, NULL);
}

static void dg_window_unload(Window *window) {
  if (s_dg_timer) { app_timer_cancel(s_dg_timer); s_dg_timer = NULL; }
  layer_destroy(s_dg_layer); s_dg_layer = NULL;
  window_destroy(s_dg_window); s_dg_window = NULL;
}

static void minigame_push_dodge(void) {
  s_dg_lane = 1;
  s_dg_lives = 3;
  s_dg_ticks = 0;
  s_dg_done = false;
  s_dg_won = false;
  memset(s_dg_rocks, 0, sizeof(s_dg_rocks));

  s_dg_window = window_create();
  window_set_background_color(s_dg_window, GColorBlack);
  window_set_window_handlers(s_dg_window, (WindowHandlers) {
    .load   = dg_window_load,
    .unload = dg_window_unload
  });
  window_stack_push(s_dg_window, true);
}

// ---------------------------------------------------------------------------
// PLACEHOLDER PUSH FUNCTIONS (filled in Tasks 6-8)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// CATCH MINI-GAME (DEX)
// ---------------------------------------------------------------------------
#define CATCH_TICK_MS   80
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
  ui_draw_time(ctx, bounds);

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
  s_ct_catch_zone_h = 20 + (int16_t)(s_ct_dex / 10);
  if (s_ct_catch_zone_h > 60) s_ct_catch_zone_h = 60;
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
  ui_draw_fox_placeholder(ctx, GPoint(w / 2, h - 16), 0);

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
    s_ct_item_y += 4;

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
    vibes_short_pulse();
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
  Pet pet; pet_load(&pet);
  s_ct_dex = pet.dex;
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
  if (s_ct_timer) { app_timer_cancel(s_ct_timer); s_ct_timer = NULL; }
  layer_destroy(s_ct_layer); s_ct_layer = NULL;
  window_destroy(s_ct_window); s_ct_window = NULL;
}

static void minigame_push_catch(void) {
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
static void minigame_push_riddle(void)   { /* Task 7 */ }
static void minigame_push_treasure(void) { /* Task 8 */ }
