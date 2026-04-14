#include "ui.h"
#include <pebble.h>

void ui_draw_time(GContext *ctx, GRect bounds) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char buf[6];
  strftime(buf, sizeof(buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(4, 2, 60, 22),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
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
    GRect(bounds.size.w - 36, 4, 34, 16),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

void ui_draw_fox_placeholder(GContext *ctx, GPoint center, uint8_t frame) {
  // Body
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_fill_circle(ctx, center, 8);

  // Ears
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(center.x - 5, center.y - 8),  GPoint(center.x - 8, center.y - 14));
  graphics_draw_line(ctx, GPoint(center.x - 8, center.y - 14), GPoint(center.x - 2, center.y - 10));
  graphics_draw_line(ctx, GPoint(center.x + 5, center.y - 8),  GPoint(center.x + 8, center.y - 14));
  graphics_draw_line(ctx, GPoint(center.x + 8, center.y - 14), GPoint(center.x + 2, center.y - 10));

  // Eyes
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(center.x - 3, center.y - 2), 1);
  graphics_fill_circle(ctx, GPoint(center.x + 3, center.y - 2), 1);

  // Tail wag (alternates y offset by frame)
  int8_t tail_dy = (frame % 2 == 0) ? 2 : -2;
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(center.x - 8, center.y + 4),
                          GPoint(center.x - 14, center.y + 4 + tail_dy));
}
