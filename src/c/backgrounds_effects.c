/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "backgrounds.h"
#include "shared_types.h"
#include <pebble.h>

// Deterministic hash — same input always yields the same output.
// Used to pick stable particle positions from (scroll_offset, index).
static inline uint16_t bg_hash(uint16_t a, uint16_t b) {
  uint32_t h = ((uint32_t)a * 73856093u) ^ ((uint32_t)b * 19349663u);
  return (uint16_t)(h ^ (h >> 16));
}

// ---------------------------------------------------------------------------
// Per-biome procedural effect overlays
// ---------------------------------------------------------------------------

static void effects_plains(GContext *ctx, GRect area, uint8_t tick) {
  // Drifting clouds: each cloud is a cluster of overlapping filled circles
  // to create a fluffy shape. Two clouds drift right-to-left across the sky.
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (uint8_t i = 0; i < 2; i++) {
    uint16_t x_seed = bg_hash(i, 0);
    int16_t cx = (int16_t)(((uint32_t)x_seed + (uint32_t)area.size.w * 256
                            - (uint32_t)(tick)) % area.size.w) + area.origin.x;
    int16_t cy = area.origin.y + 18 + (int16_t)(bg_hash(i, 1) % 8);
    // Draw a cloud shape: wide flat cluster of circles
    graphics_fill_circle(ctx, GPoint(cx,     cy),     4);  // center
    graphics_fill_circle(ctx, GPoint(cx - 5, cy + 1), 3);  // left
    graphics_fill_circle(ctx, GPoint(cx + 5, cy + 1), 3);  // right
    graphics_fill_circle(ctx, GPoint(cx - 2, cy - 2), 3);  // top-left
    graphics_fill_circle(ctx, GPoint(cx + 2, cy - 2), 3);  // top-right
  }

  // Pollen/seed particles: tiny yellow specks drifting right-to-left
  // in the middle band between sky and ground.
  if (area.size.h > 60) {
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    for (uint8_t i = 0; i < 4; i++) {
      uint16_t x_seed = bg_hash(i, 2);
      int16_t px = (int16_t)(((uint32_t)x_seed + (uint32_t)area.size.w * 256
                              - (uint32_t)(tick * 2)) % area.size.w) + area.origin.x;
      int16_t py = area.origin.y + 60 + (int16_t)(bg_hash(i, 3) % (area.size.h / 3));
      graphics_fill_circle(ctx, GPoint(px, py), 1);
    }
  }
}

static void effects_forest(GContext *ctx, GRect area, uint8_t tick) {
  // Falling leaves: small leaf shapes drifting down with horizontal sway.
  for (uint8_t i = 0; i < 6; i++) {
    // Base x drifts right-to-left over time (subtract tick).
    // First leaf is seeded in the left 40% to ensure coverage.
    uint16_t x_range = (i == 0) ? (area.size.w * 2 / 5) : area.size.w;
    int16_t x0 = (int16_t)(((uint32_t)bg_hash(i, 10) + (uint32_t)area.size.w * 256
                            - (uint32_t)(tick)) % x_range) + area.origin.x;
    int16_t sway = (int16_t)((tick + i * 3) % 12) - 6;
    int16_t y = (int16_t)((bg_hash(i, 11) + tick * 2) % area.size.h) + area.origin.y;
    int16_t lx = x0 + sway;

    // Alternate leaf colors: orange and yellow-green
    GColor leaf_color = (i % 2 == 0)
      ? PBL_IF_COLOR_ELSE(GColorOrange, GColorLightGray)
      : PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite);
    graphics_context_set_stroke_color(ctx, leaf_color);
    graphics_context_set_fill_color(ctx, leaf_color);

    // Draw leaf as a small diamond/oval: center dot + 4 directional pixels
    // The shape tilts based on sway direction (left or right lean)
    int16_t tilt = (sway > 0) ? 1 : -1;
    graphics_fill_circle(ctx, GPoint(lx, y), 1);            // center 3x3
    graphics_draw_pixel(ctx, GPoint(lx + tilt * 2, y - 1)); // tip
    graphics_draw_pixel(ctx, GPoint(lx - tilt * 2, y + 1)); // tail
  }
}

static void effects_water(GContext *ctx, GRect area, uint8_t tick) {
  // Rising bubble clusters: 3 groups of 3-6 bubbles each, rising upward.
  // Spread across the full width. Bubbles stay within the biome area bounds.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
  for (uint8_t g = 0; g < 3; g++) {
    // Spread groups evenly across the width (thirds) with some variation
    int16_t base_x = area.origin.x + (int16_t)(area.size.w * g / 3)
                   + (int16_t)(bg_hash(g, 20) % (area.size.w / 3));
    uint8_t count = 3 + (bg_hash(g, 24) % 4);  // 3-6 bubbles per group
    for (uint8_t b = 0; b < count; b++) {
      int16_t ox = (int16_t)(bg_hash(g * 7 + b, 25) % 7) - 3;  // -3..+3 px offset
      int16_t oy = (int16_t)(bg_hash(g * 7 + b, 26) % 10);     // 0..9 px stagger
      int16_t bx = base_x + ox;
      // Rise from bottom, clamped to stay within the biome area
      int16_t rise = (int16_t)((bg_hash(g * 7 + b, 21) + tick * 4) % area.size.h);
      int16_t by = area.origin.y + area.size.h - 4 - rise - oy;
      if (by < area.origin.y) continue;  // don't draw above biome area
      graphics_draw_circle(ctx, GPoint(bx, by), 1);
    }
  }

  // Light caustics: diagonal bright lines spanning full biome height,
  // leaning and drifting right-to-left.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorLightGray));
  for (uint8_t i = 0; i < 3; i++) {
    int16_t cx = (int16_t)(((uint32_t)bg_hash(i, 22) + (uint32_t)area.size.w * 256
                            - (uint32_t)(tick * 2)) % area.size.w) + area.origin.x;
    graphics_draw_line(ctx,
      GPoint(cx,      area.origin.y),
      GPoint(cx - 8,  area.origin.y + area.size.h));
  }
}

static void effects_mountain(GContext *ctx, GRect area, uint8_t tick) {
  // Drifting snowflakes — white filled circle with a black outline so
  // they're visible against the white mountain peaks. Drift right-to-left.
  for (uint8_t i = 0; i < 8; i++) {
    int16_t sx = (int16_t)(((uint32_t)bg_hash(i, 30) + (uint32_t)area.size.w * 256
                            - (uint32_t)(tick)) % area.size.w) + area.origin.x;
    int16_t sy = (int16_t)((bg_hash(i, 31) + tick * 2) % area.size.h) + area.origin.y;
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_circle(ctx, GPoint(sx, sy), 1);        // black outline
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(sx, sy), 0);        // white center pixel
  }
}

static void effects_cave(GContext *ctx, GRect area, uint8_t tick) {
  int16_t ground = backgrounds_ground_y(area, BIOME_CAVE);
  int16_t top_20 = area.origin.y + 20;
  int16_t sparkle_range = ground - 5 - top_20;  // 20px from top to 5px above ground
  int16_t drip_range = ground - top_20;          // 20px from top to ground

  // Twinkling crystal sparkles — 3 sparkles that each linger for 3 frames
  // then jump to a new random position. Using tick/3 as the time seed means
  // positions hold steady for 3 consecutive frames before changing.
  if (sparkle_range > 0) {
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
    uint8_t slow_tick = tick / 3;
    for (uint8_t i = 0; i < 3; i++) {
      int16_t cx = (int16_t)(bg_hash(slow_tick * 5 + i, 40) % area.size.w) + area.origin.x;
      int16_t cy = top_20 + (int16_t)(bg_hash(slow_tick * 5 + i, 41) % sparkle_range);
      graphics_fill_circle(ctx, GPoint(cx, cy), 1);
    }
  }

  // Water drips — 2 blue drips falling straight down from top to ground.
  // x is locked for the entire fall and only changes when a new cycle starts.
  if (drip_range > 0) {
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorBlueMoon, GColorWhite));
    for (uint8_t d = 0; d < 2; d++) {
      uint8_t cycle = 20 + (bg_hash(d, 43) % 10);  // 20-29 ticks per cycle
      uint16_t offset = bg_hash(d, 44);
      uint16_t adjusted = (uint16_t)(tick + offset);
      uint8_t phase = adjusted % cycle;
      uint16_t cycle_idx = adjusted / cycle;
      int16_t dx = (int16_t)(bg_hash(d + cycle_idx * 2, 42) % area.size.w) + area.origin.x;
      // y falls from top_20 straight down to ground
      int16_t dy = top_20 + (int16_t)((uint32_t)phase * drip_range / cycle);
      int16_t streak_end = dy + 6;
      if (streak_end > ground) streak_end = ground;
      graphics_draw_line(ctx, GPoint(dx, dy), GPoint(dx, streak_end));
    }
  }
}

static void effects_storm(GContext *ctx, GRect area, uint8_t tick) {
  // Dense diagonal rain streaks drifting right-to-left (wind from the right).
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
  graphics_context_set_stroke_width(ctx, 1);
  for (uint8_t i = 0; i < 30; i++) {
    int16_t rx = (int16_t)(((uint32_t)bg_hash(i, 50) + (uint32_t)area.size.w * 256
                            - (uint32_t)(tick * 3)) % area.size.w) + area.origin.x;
    int16_t ry = (int16_t)((bg_hash(i, 51) + tick * 6) % area.size.h) + area.origin.y;
    graphics_draw_line(ctx, GPoint(rx, ry), GPoint(rx - 2, ry + 5));
  }

  // Lightning — strikes every ~25 ticks, visible for 2 ticks.
  uint8_t cycle = tick % 25;
  if (cycle < 2) {
    uint16_t bolt_seed = bg_hash((uint16_t)(tick / 25), 55);
    int16_t bx = area.origin.x + 20 + (int16_t)(bolt_seed % (area.size.w - 40));
    int16_t by = area.origin.y;

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 1);

    int16_t x = bx, y = by;
    for (int seg = 0; seg < 5; seg++) {
      int16_t dx = (int16_t)((bg_hash(bolt_seed + seg, 56) % 11) - 5);
      int16_t dy = (int16_t)(8 + bg_hash(bolt_seed + seg, 57) % 6);
      int16_t nx = x + dx, ny = y + dy;
      graphics_draw_line(ctx, GPoint(x, y), GPoint(nx, ny));
      x = nx; y = ny;
    }

    if (cycle == 0) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, GPoint(bx, by + 4), 3);
    }
  }
  graphics_context_set_stroke_width(ctx, 1);
}

void backgrounds_draw_effects(GContext *ctx, GRect area, uint8_t biome,
                              uint16_t scroll_offset, uint8_t effect_tick) {
  (void)scroll_offset;  // reserved for future parallax effects
  switch ((BiomeType)biome) {
    case BIOME_PLAINS:   effects_plains(ctx, area, effect_tick); break;
    case BIOME_FOREST:   effects_forest(ctx, area, effect_tick); break;
    case BIOME_WATER:    effects_water(ctx, area, effect_tick); break;
    case BIOME_MOUNTAIN: effects_mountain(ctx, area, effect_tick); break;
    case BIOME_CAVE:     effects_cave(ctx, area, effect_tick); break;
    case BIOME_STORM:    effects_storm(ctx, area, effect_tick); break;
    default: break;
  }
}
