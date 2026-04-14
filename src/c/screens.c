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
    if (len == 0) return;  // Don't allow empty name
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
// ---------------------------------------------------------------------------
// SCREEN_MAIN
// ---------------------------------------------------------------------------
static Window   *s_main_window     = NULL;
static Layer    *s_main_layer      = NULL;
static AppTimer *s_main_anim_timer = NULL;
static uint8_t   s_main_fox_frame  = 0;
static Pet       s_main_pet;
static bool      s_main_has_active_adv = false;

static void main_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  ui_draw_time(ctx, bounds);
  ui_draw_battery(ctx, bounds);

  // Fox in upper quarter
  GPoint fox_center = GPoint(w / 2, h / 4 + 4);
  ui_draw_fox_placeholder(ctx, fox_center, s_main_fox_frame);

  // Name + level
  char name_buf[20];
  snprintf(name_buf, sizeof(name_buf), "%s  Lv.%d",
           s_main_pet.name, (int)s_main_pet.level);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, name_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, h / 4 + 20, w, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // XP bar
  ui_draw_progress_bar(ctx,
    GRect(8, h / 4 + 40, w - 16, 8),
    s_main_pet.xp, s_main_pet.xp_next_level);

  // Stats — 2 columns
  static const char *labels[NUM_STATS] = { "STR", "DEX", "AGI", "VIT", "INT", "LUK" };
  int16_t stats_top = h / 4 + 54;
  for (int i = 0; i < NUM_STATS; i++) {
    int16_t col_x = (i % 2 == 0) ? 4 : w / 2 + 4;
    int16_t row_y = stats_top + (int16_t)(i / 2) * 17;
    char sbuf[12];
    snprintf(sbuf, sizeof(sbuf), "%s:%d", labels[i], (int)pet_get_stat(&s_main_pet, i));
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, sbuf,
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(col_x, row_y, w / 2 - 6, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Bottom action hint — uses cached state, refreshed on window_load and worker messages
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, s_main_has_active_adv ? "SELECT: Resume" : "SELECT: Adventure",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 18, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void main_anim_callback(void *ctx) {
  s_main_fox_frame = (s_main_fox_frame + 1) % 4;
  layer_mark_dirty(s_main_layer);
  s_main_anim_timer = app_timer_register(600, main_anim_callback, NULL);
}

static void main_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  Adventure adv;
  s_main_has_active_adv = adventure_load(&adv) && adv.active;
  if (!s_main_has_active_adv) {
    pet_load(&s_main_pet);
    adventure_init(&adv, &s_main_pet);
    adventure_save(&adv);
  }
  screens_push_adventure();
}

static void main_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, main_click_select);
}

static void main_window_load(Window *window) {
  pet_load(&s_main_pet);
  Adventure adv;
  s_main_has_active_adv = adventure_load(&adv) && adv.active;
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_main_layer = layer_create(bounds);
  layer_set_update_proc(s_main_layer, main_layer_update);
  layer_add_child(root, s_main_layer);
  window_set_click_config_provider(window, main_click_config);
  s_main_anim_timer = app_timer_register(600, main_anim_callback, NULL);
}

static void main_window_unload(Window *window) {
  if (s_main_anim_timer) { app_timer_cancel(s_main_anim_timer); s_main_anim_timer = NULL; }
  layer_destroy(s_main_layer); s_main_layer = NULL;
  window_destroy(s_main_window); s_main_window = NULL;
}

void screens_push_main(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
}
// ---------------------------------------------------------------------------
// SCREEN_ADVENTURE
// ---------------------------------------------------------------------------
static Window    *s_adv_window     = NULL;
static Layer     *s_adv_layer      = NULL;
static AppTimer  *s_adv_anim_timer = NULL;
static uint8_t    s_adv_fox_frame  = 0;
static Adventure  s_adv_current;
static Pet        s_adv_pet;
static bool       s_adv_show_info  = false;

static const char *s_biome_names[NUM_BIOMES] = {
  "Plains", "Forest", "Water", "Mountain", "Cave", "Storm"
};

static void adv_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;
  int16_t h = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Time — always visible top-left
  ui_draw_time(ctx, bounds);

  if (adventure_is_complete(&s_adv_current)) {
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_text(ctx, "ADVENTURE DONE!",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, h / 2 - 22, w, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "SELECT: Results",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(0, h / 2 + 6, w, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  uint8_t seg = s_adv_current.current_segment;

  // Segment counter top-right
  char seg_buf[12];
  snprintf(seg_buf, sizeof(seg_buf), "Seg %d/%d", (int)(seg + 1), (int)s_adv_current.num_segments);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, seg_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(w - 62, 2, 60, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  // Progress bar
  uint8_t pct = adventure_segment_progress_pct(&s_adv_current);
  ui_draw_progress_bar(ctx, GRect(8, 22, w - 16, 10), (uint32_t)pct, 100);

  // Biome + pct label
  const char *bname = (s_adv_current.segments[seg] < NUM_BIOMES) ? s_biome_names[s_adv_current.segments[seg]] : "?";
  char biome_buf[20];
  snprintf(biome_buf, sizeof(biome_buf), "%s  %d%%", bname, (int)pct);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite));
  graphics_draw_text(ctx, biome_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, 34, w - 8, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Ground line
  int16_t ground_y = h / 2 + 8;
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(0, ground_y), GPoint(w, ground_y));

  // Walking fox
  ui_draw_fox_placeholder(ctx, GPoint(w / 3, ground_y - 12), s_adv_fox_frame);

  // Steps today
  uint32_t steps = (uint32_t)health_service_sum_today(HealthMetricStepCount);
  char steps_buf[20];
  snprintf(steps_buf, sizeof(steps_buf), "Steps: %lu", (unsigned long)steps);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, steps_buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, h - 34, w - 8, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Button hints
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "UP:info  DN:games",
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(0, h - 18, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Info overlay (toggled by Up) — uses cached s_adv_pet, loaded on window_load
  if (s_adv_show_info) {
    GRect info = GRect(8, 50, w - 16, 68);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    graphics_fill_rect(ctx, info, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_round_rect(ctx, info, 4);

    char ibuf[32];
    snprintf(ibuf, sizeof(ibuf), "%s  Lv.%d", s_adv_pet.name, (int)s_adv_pet.level);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, ibuf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(12, 53, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    snprintf(ibuf, sizeof(ibuf), "XP %lu/%lu", (unsigned long)s_adv_pet.xp, (unsigned long)s_adv_pet.xp_next_level);
    graphics_draw_text(ctx, ibuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(12, 70, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    snprintf(ibuf, sizeof(ibuf), "S%d D%d A%d V%d I%d L%d",
             (int)s_adv_pet.str, (int)s_adv_pet.dex, (int)s_adv_pet.agi,
             (int)s_adv_pet.vit, (int)s_adv_pet.intel, (int)s_adv_pet.luk);
    graphics_draw_text(ctx, ibuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(12, 88, w - 24, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void adv_anim_callback(void *ctx) {
  s_adv_fox_frame = (s_adv_fox_frame + 1) % 4;
  // Don't poll persist here — s_adv_current is refreshed by screens_on_worker_message
  layer_mark_dirty(s_adv_layer);
  s_adv_anim_timer = app_timer_register(300, adv_anim_callback, NULL);
}

static void adv_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  s_adv_show_info = !s_adv_show_info;
  layer_mark_dirty(s_adv_layer);
}

static void adv_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (adventure_is_complete(&s_adv_current)) {
    window_stack_pop(true);
    screens_push_results(s_adv_current.total_xp_earned, s_adv_current.encounters_total);
  }
}

static void adv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,    adv_click_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, adv_click_select);
  // Down reserved for mini-games (Plan 2)
}

static void adv_window_load(Window *window) {
  adventure_load(&s_adv_current);
  pet_load(&s_adv_pet);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_adv_layer = layer_create(bounds);
  layer_set_update_proc(s_adv_layer, adv_layer_update);
  layer_add_child(root, s_adv_layer);
  window_set_click_config_provider(window, adv_click_config);
  s_adv_anim_timer = app_timer_register(300, adv_anim_callback, NULL);
}

static void adv_window_unload(Window *window) {
  if (s_adv_anim_timer) { app_timer_cancel(s_adv_anim_timer); s_adv_anim_timer = NULL; }
  layer_destroy(s_adv_layer); s_adv_layer = NULL;
  window_destroy(s_adv_window); s_adv_window = NULL;
}

void screens_push_adventure(void) {
  s_adv_show_info = false;
  s_adv_window = window_create();
  window_set_background_color(s_adv_window, GColorBlack);
  window_set_window_handlers(s_adv_window, (WindowHandlers) {
    .load   = adv_window_load,
    .unload = adv_window_unload
  });
  window_stack_push(s_adv_window, true);
}

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

  // XP progress bar for current level — uses cached s_res_pet from window_load
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
  window_stack_pop(true);
  if (s_res_lvls > 0) {
    screens_push_levelup(&s_res_pet);
  } else {
    screens_push_main();
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

// ---------------------------------------------------------------------------
// SCREEN_LEVELUP
// ---------------------------------------------------------------------------
static Window  *s_lu_window  = NULL;
static Layer   *s_lu_layer   = NULL;
static Pet      s_lu_pet;
static uint8_t  s_lu_row     = 0;
static uint16_t s_lu_alloc[NUM_STATS];

static void lu_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int16_t w = bounds.size.w;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  ui_draw_time(ctx, bounds);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
  graphics_draw_text(ctx, "LEVEL UP!",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(0, 24, w, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char pts_buf[20];
  snprintf(pts_buf, sizeof(pts_buf), "Points: %d", (int)s_lu_pet.upgrade_points);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, pts_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, 44, w, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  for (uint8_t i = 0; i < NUM_STATS; i++) {
    int16_t row_y = 64 + (int16_t)(i * 16);
    bool sel = (s_lu_row == i);
    if (sel) {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(0, row_y - 1, w, 15), 0, GCornerNone);
    }
    char sbuf[20];
    snprintf(sbuf, sizeof(sbuf), "%s: %d (+%d)",
             s_stat_names[i], (int)pet_get_stat(&s_lu_pet, i), (int)s_lu_alloc[i]);
    graphics_context_set_text_color(ctx,
      sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorWhite);
    graphics_draw_text(ctx, sbuf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(4, row_y, w - 8, 14),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  bool done_sel = (s_lu_row == NUM_STATS);
  graphics_context_set_text_color(ctx,
    done_sel ? PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite) : GColorLightGray);
  graphics_draw_text(ctx, done_sel ? "> DONE <" : "  DONE  ",
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, 64 + NUM_STATS * 16, w, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void lu_click_up(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row > 0) s_lu_row--;
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_down(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row < NUM_STATS) s_lu_row++;
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_select(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row < NUM_STATS) {
    uint16_t val = pet_get_stat(&s_lu_pet, s_lu_row);
    if (stats_raise_stat(&val, &s_lu_pet.upgrade_points)) {
      pet_set_stat(&s_lu_pet, s_lu_row, val);
      s_lu_alloc[s_lu_row]++;
    }
  } else {
    pet_save(&s_lu_pet);
    window_stack_pop(true);
    screens_push_main();
  }
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_back(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (s_lu_row < NUM_STATS && s_lu_alloc[s_lu_row] > 0) {
    uint16_t val = pet_get_stat(&s_lu_pet, s_lu_row);
    if (stats_lower_stat(&val, &s_lu_pet.upgrade_points)) {
      pet_set_stat(&s_lu_pet, s_lu_row, val);
      s_lu_alloc[s_lu_row]--;
    }
  }
  layer_mark_dirty(s_lu_layer);
}

static void lu_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,    lu_click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN,  lu_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, lu_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK,   lu_click_back);
}

static void lu_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_lu_layer = layer_create(bounds);
  layer_set_update_proc(s_lu_layer, lu_layer_update);
  layer_add_child(root, s_lu_layer);
  window_set_click_config_provider(window, lu_click_config);
}

static void lu_window_unload(Window *window) {
  layer_destroy(s_lu_layer); s_lu_layer = NULL;
  window_destroy(s_lu_window); s_lu_window = NULL;
}

void screens_push_levelup(Pet *pet) {
  s_lu_pet = *pet;
  s_lu_row = 0;
  memset(s_lu_alloc, 0, sizeof(s_lu_alloc));

  s_lu_window = window_create();
  window_set_background_color(s_lu_window, GColorBlack);
  window_set_window_handlers(s_lu_window, (WindowHandlers) {
    .load   = lu_window_load,
    .unload = lu_window_unload
  });
  window_stack_push(s_lu_window, true);
}

void screens_on_worker_message(uint16_t type, AppWorkerMessage *message) {
  (void)message;
  adventure_load(&s_adv_current);
  // Update main screen's cached adventure state
  s_main_has_active_adv = s_adv_current.active;
  if (s_adv_layer)  layer_mark_dirty(s_adv_layer);
  if (s_main_layer) layer_mark_dirty(s_main_layer);
  if (type == WORKER_MSG_ADVENTURE_DONE) {
    vibes_short_pulse();
  }
}
