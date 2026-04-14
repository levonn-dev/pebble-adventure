#include "ui.h"
#include "sprites.h"
#include "shared_types.h"
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
  // Try PNG sprite first — falls back to procedural if not loaded
  GBitmap *sprite = sprites_get_fox(state, frame);
  if (sprite) {
    GRect dest = GRect(center.x - 8, center.y - 8, 16, 16);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, sprite, dest);
    return;
  }

  // Procedural fallback
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

// ---------------------------------------------------------------------------
// Biome backgrounds — procedural per-biome
// ---------------------------------------------------------------------------

static void biome_draw_ground(GContext *ctx, GRect area, GColor sky, GColor ground) {
  int16_t horizon = area.origin.y + area.size.h * 2 / 3;
  graphics_context_set_fill_color(ctx, sky);
  graphics_fill_rect(ctx, GRect(area.origin.x, area.origin.y,
                                 area.size.w, horizon - area.origin.y), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, ground);
  graphics_fill_rect(ctx, GRect(area.origin.x, horizon,
                                 area.size.w, area.origin.y + area.size.h - horizon), 0, GCornerNone);
}

void ui_draw_biome_bg(GContext *ctx, GRect area, uint8_t biome, uint8_t frame) {
  int16_t x = area.origin.x;
  int16_t y = area.origin.y;
  int16_t w = area.size.w;
  int16_t h = area.size.h;
  int16_t horizon = y + h * 2 / 3;

  switch ((BiomeType)biome) {
    case BIOME_PLAINS: {
      biome_draw_ground(ctx, area,
        PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack),
        PBL_IF_COLOR_ELSE(GColorGreen, GColorDarkGray));
      // Grass tufts
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorWhite));
      graphics_context_set_stroke_width(ctx, 1);
      for (int16_t gx = x + 8; gx < x + w; gx += 14) {
        int16_t gy = horizon + 4 + (gx * 3) % 7;
        graphics_draw_line(ctx, GPoint(gx, gy), GPoint(gx - 2, gy - 4));
        graphics_draw_line(ctx, GPoint(gx, gy), GPoint(gx + 2, gy - 3));
      }
      break;
    }

    case BIOME_FOREST: {
      biome_draw_ground(ctx, area,
        PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack),
        PBL_IF_COLOR_ELSE(GColorArmyGreen, GColorDarkGray));
      // Trees
      for (int16_t tx = x + 10; tx < x + w; tx += 20) {
        int16_t ty = horizon - 6 - (tx * 7) % 12;
        // Trunk
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorBulgarianRose, GColorWhite));
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(tx, horizon), GPoint(tx, ty + 6));
        // Canopy
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorLightGray));
        for (int16_t r = 0; r < 8; r++) {
          int16_t half = r * 5 / 8;
          graphics_draw_line(ctx, GPoint(tx - half, ty + r), GPoint(tx + half, ty + r));
        }
      }
      break;
    }

    case BIOME_WATER: {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorBlack));
      graphics_fill_rect(ctx, area, 0, GCornerNone);
      // Waves
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorLightGray));
      graphics_context_set_stroke_width(ctx, 1);
      for (int16_t wy = y + 8; wy < y + h; wy += 10) {
        int16_t offset = (frame * 3 + wy) % 12;
        for (int16_t wx = x - 4 + offset; wx < x + w; wx += 12) {
          graphics_draw_line(ctx, GPoint(wx, wy), GPoint(wx + 4, wy - 2));
          graphics_draw_line(ctx, GPoint(wx + 4, wy - 2), GPoint(wx + 8, wy));
        }
      }
      // Ground strip
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorDarkGray));
      graphics_fill_rect(ctx, GRect(x, horizon, w, y + h - horizon), 0, GCornerNone);
      break;
    }

    case BIOME_MOUNTAIN: {
      biome_draw_ground(ctx, area,
        PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack),
        PBL_IF_COLOR_ELSE(GColorBulgarianRose, GColorDarkGray));
      // Mountain peaks
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorLightGray));
      for (int16_t mx = x; mx < x + w; mx += 30) {
        int16_t peak_h = 16 + (mx * 11) % 14;
        for (int16_t r = 0; r < peak_h; r++) {
          int16_t half = r * 12 / peak_h;
          int16_t py = horizon - peak_h + r;
          if (py >= y) {
            graphics_draw_line(ctx, GPoint(mx + 15 - half, py), GPoint(mx + 15 + half, py));
          }
        }
      }
      #ifdef PBL_COLOR
      // Snow caps
      graphics_context_set_fill_color(ctx, GColorWhite);
      for (int16_t mx = x; mx < x + w; mx += 30) {
        int16_t peak_h = 16 + (mx * 11) % 14;
        graphics_fill_circle(ctx, GPoint(mx + 15, horizon - peak_h + 2), 3);
      }
      #endif
      break;
    }

    case BIOME_CAVE: {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
      graphics_fill_rect(ctx, area, 0, GCornerNone);
      // Rocky ground
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorDarkGray));
      graphics_fill_rect(ctx, GRect(x, horizon, w, y + h - horizon), 0, GCornerNone);
      // Stalactites
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorLightGray));
      graphics_context_set_stroke_width(ctx, 2);
      for (int16_t sx = x + 6; sx < x + w; sx += 16) {
        int16_t len = 6 + (sx * 5) % 10;
        graphics_draw_line(ctx, GPoint(sx, y), GPoint(sx, y + len));
        graphics_draw_line(ctx, GPoint(sx - 2, y), GPoint(sx, y + len));
        graphics_draw_line(ctx, GPoint(sx + 2, y), GPoint(sx, y + len));
      }
      break;
    }

    case BIOME_STORM: {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
      graphics_fill_rect(ctx, area, 0, GCornerNone);
      // Ground
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorArmyGreen, GColorDarkGray));
      graphics_fill_rect(ctx, GRect(x, horizon, w, y + h - horizon), 0, GCornerNone);
      // Rain
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorLightGray));
      graphics_context_set_stroke_width(ctx, 1);
      int16_t rain_offset = (frame * 5) % 10;
      for (int16_t ry = y + rain_offset; ry < horizon; ry += 10) {
        for (int16_t rx = x + 3 + ((ry * 7) % 20); rx < x + w; rx += 20) {
          graphics_draw_line(ctx, GPoint(rx, ry), GPoint(rx - 1, ry + 4));
        }
      }
      // Lightning (certain frames)
      if (frame % 4 == 0) {
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
        graphics_context_set_stroke_width(ctx, 2);
        int16_t lx = x + w / 3 + (frame * 13) % (w / 2);
        graphics_draw_line(ctx, GPoint(lx, y + 4), GPoint(lx - 4, y + h / 3));
        graphics_draw_line(ctx, GPoint(lx - 4, y + h / 3), GPoint(lx + 2, y + h / 3));
        graphics_draw_line(ctx, GPoint(lx + 2, y + h / 3), GPoint(lx - 2, y + h / 2));
      }
      break;
    }
  }
}
