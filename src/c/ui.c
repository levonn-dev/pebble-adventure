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

// ---------------------------------------------------------------------------
// Fox sprite — procedural multi-state drawing
// ---------------------------------------------------------------------------

static void fox_draw_body(GContext *ctx, GPoint c, int8_t body_dy) {
  // Body oval
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_fill_rect(ctx, GRect(c.x - 7, c.y + body_dy - 3, 14, 8), 3, GCornersAll);

  // White chest patch (color only)
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(c.x - 3, c.y + body_dy - 1, 6, 5), 2, GCornersAll);
  #endif

  // Head
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_fill_circle(ctx, GPoint(c.x + 4, c.y + body_dy - 6), 5);

  // Snout
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorMelon, GColorWhite));
  graphics_fill_circle(ctx, GPoint(c.x + 7, c.y + body_dy - 5), 2);

  // Nose
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(c.x + 9, c.y + body_dy - 5), 1);

  // Eyes
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(c.x + 3, c.y + body_dy - 8), 1);
}

static void fox_draw_ears(GContext *ctx, GPoint c, int8_t body_dy, int8_t ear_dy) {
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  // Left ear
  graphics_fill_circle(ctx, GPoint(c.x + 1, c.y + body_dy - 11 + ear_dy), 2);
  // Right ear
  graphics_fill_circle(ctx, GPoint(c.x + 6, c.y + body_dy - 11 + ear_dy), 2);

  // Inner ear (color only)
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorMelon);
  graphics_fill_circle(ctx, GPoint(c.x + 1, c.y + body_dy - 11 + ear_dy), 1);
  graphics_fill_circle(ctx, GPoint(c.x + 6, c.y + body_dy - 11 + ear_dy), 1);
  #endif
}

static void fox_draw_legs(GContext *ctx, GPoint c, int8_t body_dy,
                           int8_t l1, int8_t l2, int8_t l3, int8_t l4) {
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_context_set_stroke_width(ctx, 2);
  // Front legs
  graphics_draw_line(ctx, GPoint(c.x + 3, c.y + body_dy + 4),
                          GPoint(c.x + 3, c.y + body_dy + 8 + l1));
  graphics_draw_line(ctx, GPoint(c.x + 6, c.y + body_dy + 4),
                          GPoint(c.x + 6, c.y + body_dy + 8 + l2));
  // Back legs
  graphics_draw_line(ctx, GPoint(c.x - 3, c.y + body_dy + 4),
                          GPoint(c.x - 3, c.y + body_dy + 8 + l3));
  graphics_draw_line(ctx, GPoint(c.x - 6, c.y + body_dy + 4),
                          GPoint(c.x - 6, c.y + body_dy + 8 + l4));
}

static void fox_draw_tail(GContext *ctx, GPoint c, int8_t body_dy,
                           int8_t tail_dy, int8_t tail_dx) {
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite));
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(c.x - 7, c.y + body_dy),
                          GPoint(c.x - 12 + tail_dx, c.y + body_dy - 4 + tail_dy));
  // White tail tip (color only)
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(c.x - 12 + tail_dx, c.y + body_dy - 4 + tail_dy), 1);
  #endif
}

void ui_draw_fox(GContext *ctx, GPoint center, FoxState state, uint8_t frame) {
  int8_t body_dy = 0;
  int8_t ear_dy = 0;
  int8_t tail_dy = 0;
  int8_t tail_dx = 0;
  int8_t l1 = 0, l2 = 0, l3 = 0, l4 = 0;

  switch (state) {
    case FOX_IDLE:
      ear_dy = (frame % 2 == 0) ? 0 : -1;
      tail_dy = (frame % 2 == 0) ? 0 : 1;
      break;

    case FOX_WALK:
      body_dy = (frame % 2 == 0) ? 0 : -1;
      switch (frame % 4) {
        case 0: l1 = -2; l2 = 2; l3 = 2; l4 = -2; break;
        case 1: l1 = 0;  l2 = 0; l3 = 0; l4 = 0;  break;
        case 2: l1 = 2;  l2 = -2; l3 = -2; l4 = 2; break;
        case 3: l1 = 0;  l2 = 0; l3 = 0; l4 = 0;  break;
      }
      tail_dy = (frame % 2 == 0) ? -1 : 1;
      break;

    case FOX_HAPPY:
      body_dy = -2;
      ear_dy = -2;
      tail_dy = -6;
      tail_dx = 2;
      break;

    case FOX_SAD:
      body_dy = 2;
      ear_dy = 3;
      tail_dy = 4;
      break;

    case FOX_DIG:
      body_dy = 1;
      l1 = (frame % 2 == 0) ? -3 : 1;
      l2 = (frame % 2 == 0) ? 1 : -3;
      tail_dy = -2;
      break;
  }

  fox_draw_tail(ctx, center, body_dy, tail_dy, tail_dx);
  fox_draw_body(ctx, center, body_dy);
  fox_draw_ears(ctx, center, body_dy, ear_dy);
  fox_draw_legs(ctx, center, body_dy, l1, l2, l3, l4);
}

// Backward compatibility — remove after all callers are updated
void ui_draw_fox_placeholder(GContext *ctx, GPoint center, uint8_t frame) {
  ui_draw_fox(ctx, center, FOX_WALK, frame);
}
