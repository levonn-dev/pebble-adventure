#include "ui.h"
#include "sprites.h"
#include "shared_types.h"
#include <pebble.h>

void ui_draw_status_bar(GContext *ctx, GRect bounds) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char buf[6];
  strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(4, 2, 60, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ui_draw_battery(ctx, bounds);
}

void ui_draw_progress_bar(GContext *ctx, GRect rect, uint32_t value, uint32_t max_val) {
  if (max_val == 0) max_val = 1;

  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  graphics_fill_rect(ctx, rect, 2, GCornersAll);

  uint32_t fill_w = (uint32_t)rect.size.w * value / max_val;
  if (fill_w > 0) {
    GRect fill = GRect(rect.origin.x, rect.origin.y, (int16_t)fill_w, rect.size.h);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite));
    graphics_fill_rect(ctx, fill, 2, GCornersAll);
  }

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, rect, 2);
}

void ui_draw_battery(GContext *ctx, GRect bounds) {
  BatteryChargeState bat = battery_state_service_peek();
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)bat.charge_percent);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(bounds.size.w - 36, 2, 34, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

// ---------------------------------------------------------------------------
// Vibration
// ---------------------------------------------------------------------------

bool ui_vibration_enabled(void) {
  if (!persist_exists(PERSIST_KEY_VIBRATION)) return true;  // default on
  return persist_read_int(PERSIST_KEY_VIBRATION) != 0;
}

void ui_set_vibration(bool enabled) {
  persist_write_int(PERSIST_KEY_VIBRATION, enabled ? 1 : 0);
}

void ui_vibe_short(void)  { if (ui_vibration_enabled()) vibes_short_pulse(); }
void ui_vibe_double(void) { if (ui_vibration_enabled()) vibes_double_pulse(); }
void ui_vibe_long(void)   { if (ui_vibration_enabled()) vibes_long_pulse(); }

// ---------------------------------------------------------------------------
// Backlight management
// ---------------------------------------------------------------------------

static AppTimer *s_backlight_timer = NULL;

static void backlight_off_callback(void *data) {
  light_enable(false);
  s_backlight_timer = NULL;
}

void ui_keep_backlight(void) {
  light_enable(true);
  if (s_backlight_timer) app_timer_cancel(s_backlight_timer);
  s_backlight_timer = app_timer_register(30000, backlight_off_callback, NULL);
}

// ---------------------------------------------------------------------------
// Menu row highlight
// ---------------------------------------------------------------------------

GColor ui_draw_menu_row(GContext *ctx, int16_t y, int16_t w, int16_t text_h, bool selected) {
  // Row height = text height + 2px descender padding
  int16_t row_h = text_h + 3;
  if (selected) {
    // Color: dark gray bg, yellow text. B&W: white bg, black text (inverted).
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
    graphics_fill_rect(ctx, GRect(0, y, w, row_h), 0, GCornerNone);
    return PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack);
  }
  return GColorWhite;
}

// ---------------------------------------------------------------------------
// Fox sprite — PNG resource drawing
// ---------------------------------------------------------------------------

void ui_draw_fox(GContext *ctx, GPoint center, FoxState state, uint8_t frame) {
  GBitmap *sprite = sprites_get_fox(state, frame);
  if (sprite) {
    GRect dest = GRect(center.x - 12, center.y - 12, 24, 24);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, sprite, dest);
  }
}

void ui_draw_fox_large(GContext *ctx, GPoint center, FoxState state, uint8_t frame) {
  GBitmap *sprite = sprites_get_fox_large(state, frame);
  if (sprite) {
    GSize size = gbitmap_get_bounds(sprite).size;
    GRect dest = GRect(center.x - size.w / 2, center.y - size.h / 2, size.w, size.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, sprite, dest);
  }
}
