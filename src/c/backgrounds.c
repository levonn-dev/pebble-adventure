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
// Biome backgrounds — 6 wide PNGs that ping-pong scroll horizontally to
// simulate the fox's movement during an adventure.
//
// Biomes are LAZY-LOADED: only the currently-active biome's bitmap is kept
// in memory at any time. When backgrounds_draw is called with a different
// biome, the previous bitmap is destroyed and the new one loaded. This
// keeps memory footprint to one ~57KB bitmap instead of six (345KB),
// which is necessary to fit in the Pebble app heap on color platforms.
// ---------------------------------------------------------------------------
#define BG_NONE 0xFF  // sentinel indicating no biome is currently loaded

static GBitmap *s_current_bg    = NULL;
static uint8_t  s_current_biome = BG_NONE;

static uint32_t bg_resource_id(uint8_t biome) {
  switch ((BiomeType)biome) {
    case BIOME_PLAINS:   return RESOURCE_ID_BG_PLAINS;
    case BIOME_FOREST:   return RESOURCE_ID_BG_FOREST;
    case BIOME_WATER:    return RESOURCE_ID_BG_WATER;
    case BIOME_MOUNTAIN: return RESOURCE_ID_BG_MOUNTAIN;
    case BIOME_CAVE:     return RESOURCE_ID_BG_CAVE;
    case BIOME_STORM:    return RESOURCE_ID_BG_STORM;
    default: return 0;
  }
}

// Ensure the given biome's bitmap is loaded, freeing any previously-loaded
// biome first. No-op if the requested biome is already loaded.
static void ensure_loaded(uint8_t biome) {
  if (biome == s_current_biome && s_current_bg) return;

  if (s_current_bg) {
    gbitmap_destroy(s_current_bg);
    s_current_bg = NULL;
  }
  s_current_biome = BG_NONE;

  uint32_t rid = bg_resource_id(biome);
  if (rid == 0) return;

  s_current_bg = gbitmap_create_with_resource(rid);
  if (s_current_bg) {
    s_current_biome = biome;
  }
}

void backgrounds_init(void) {
  // Nothing to do — biome bitmaps are lazy-loaded on first draw.
}

void backgrounds_deinit(void) {
  if (s_current_bg) {
    gbitmap_destroy(s_current_bg);
    s_current_bg = NULL;
  }
  s_current_biome = BG_NONE;
}

static void draw_fallback(GContext *ctx, GRect area, uint8_t biome) {
  GColor fallback;
  switch ((BiomeType)biome) {
    case BIOME_PLAINS:   fallback = PBL_IF_COLOR_ELSE(GColorGreen,       GColorDarkGray); break;
    case BIOME_FOREST:   fallback = PBL_IF_COLOR_ELSE(GColorDarkGreen,   GColorDarkGray); break;
    case BIOME_WATER:    fallback = PBL_IF_COLOR_ELSE(GColorCeleste,     GColorLightGray); break;
    case BIOME_MOUNTAIN: fallback = PBL_IF_COLOR_ELSE(GColorLightGray,   GColorLightGray); break;
    case BIOME_CAVE:     fallback = PBL_IF_COLOR_ELSE(GColorOxfordBlue,  GColorBlack); break;
    case BIOME_STORM:    fallback = PBL_IF_COLOR_ELSE(GColorDarkGray,    GColorBlack); break;
    default:             fallback = GColorBlack; break;
  }
  graphics_context_set_fill_color(ctx, fallback);
  graphics_fill_rect(ctx, area, 0, GCornerNone);
}

void backgrounds_draw(GContext *ctx, GRect area, uint8_t biome) {
  ensure_loaded(biome);
  if (!s_current_bg) {
    draw_fallback(ctx, area, biome);
    return;
  }

  GSize img = gbitmap_get_bounds(s_current_bg).size;
  int16_t draw_w = img.w < area.size.w ? img.w : area.size.w;

  // Align the bitmap to the BOTTOM of the area so the ground strip
  // lines up with where the fox walks. If the image is taller than the
  // area, trim from the top via a sub-bitmap. If shorter, fill the
  // gap above with the fallback color (sky).
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  if (img.h >= area.size.h) {
    // Image is taller (or equal) — crop from top, draw full area height
    int16_t trim = img.h - area.size.h;
    GBitmap *sub = gbitmap_create_as_sub_bitmap(s_current_bg,
      GRect(0, trim, draw_w, area.size.h));
    if (sub) {
      graphics_draw_bitmap_in_rect(ctx, sub, area);
      gbitmap_destroy(sub);
    } else {
      draw_fallback(ctx, area, biome);
    }
  } else {
    // Image is shorter — fill sky color above, draw image at bottom
    int16_t gap = area.size.h - img.h;
    draw_fallback(ctx, GRect(area.origin.x, area.origin.y, area.size.w, gap), biome);
    GRect dest = GRect(area.origin.x, area.origin.y + gap, draw_w, img.h);
    graphics_draw_bitmap_in_rect(ctx, s_current_bg, dest);
  }
}

// ---------------------------------------------------------------------------
// Per-biome ground surface offset — pixels from the BOTTOM of the biome
// area to where the ground surface is (where the fox's paws should land).
// Tune these values per biome to match each background image's ground line.
// ---------------------------------------------------------------------------
static int16_t bg_ground_offset(uint8_t biome) {
  switch ((BiomeType)biome) {
    case BIOME_PLAINS:   return 24;
    case BIOME_FOREST:   return 22;
    case BIOME_WATER:    return 22;
    case BIOME_MOUNTAIN: return 18;
    case BIOME_CAVE:     return 18;
    case BIOME_STORM:    return 22;
    default:             return 20;
  }
}

int16_t backgrounds_ground_y(GRect area, uint8_t biome) {
  return area.origin.y + area.size.h - bg_ground_offset(biome);
}

// ---------------------------------------------------------------------------
// Procedural effect overlays — drawn on top of the background bitmap.
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
    for (uint8_t i = 0; i < 8; i++) {
      uint16_t x_seed = bg_hash(i, 2);
      int16_t px = (int16_t)(((uint32_t)x_seed + (uint32_t)area.size.w * 256
                              - (uint32_t)(tick * 2)) % area.size.w) + area.origin.x;
      int16_t py = area.origin.y + 30 + (int16_t)(bg_hash(i, 3) % (area.size.h / 3));
      graphics_fill_circle(ctx, GPoint(px, py), 0);  // single pixel but via fill for visibility
    }
  }
}

static void effects_forest(GContext *ctx, GRect area, uint8_t tick) {
  // Falling leaves: small colored dots drifting downward with slight horizontal sway.
  for (uint8_t i = 0; i < 6; i++) {
    int16_t x0 = (int16_t)(bg_hash(i, 10) % area.size.w) + area.origin.x;
    int16_t sway = (int16_t)((tick + i * 3) % 8) - 4;
    int16_t y  = (int16_t)((bg_hash(i, 11) + tick * 2) % area.size.h) + area.origin.y;
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorLightGray));
    graphics_fill_circle(ctx, GPoint(x0 + sway, y), 1);
  }
}

static void effects_water(GContext *ctx, GRect area, uint8_t tick) {
  // Rising bubbles: tiny circles drifting upward at fixed x columns.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
  for (uint8_t i = 0; i < 5; i++) {
    int16_t bx = (int16_t)(bg_hash(i, 20) % area.size.w) + area.origin.x;
    int16_t by = area.origin.y + area.size.h - 4
               - (int16_t)((bg_hash(i, 21) + tick * 4) % area.size.h);
    graphics_draw_circle(ctx, GPoint(bx, by), 1);
  }
  // Light caustics: thin diagonal bright lines that drift horizontally.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorLightGray));
  for (uint8_t i = 0; i < 3; i++) {
    // Wrap into [0, area.size.w) so caustics stay inside the area bounds.
    int16_t cx = (int16_t)((bg_hash(i, 22) + tick * 2) % area.size.w) + area.origin.x;
    graphics_draw_line(ctx,
      GPoint(cx,      area.origin.y),
      GPoint(cx + 6,  area.origin.y + area.size.h / 2));
  }
}

static void effects_mountain(GContext *ctx, GRect area, uint8_t tick) {
  // Drifting snow particles — tiny white pixels falling slowly with a slight drift.
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (uint8_t i = 0; i < 8; i++) {
    int16_t sx = (int16_t)((bg_hash(i, 30) + tick) % area.size.w) + area.origin.x;
    int16_t sy = (int16_t)((bg_hash(i, 31) + tick * 2) % area.size.h) + area.origin.y;
    graphics_draw_pixel(ctx, GPoint(sx, sy));
  }
}

static void effects_cave(GContext *ctx, GRect area, uint8_t tick) {
  // Twinkling crystal sparkles: a few pixels that flicker on/off based on tick.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
  for (uint8_t i = 0; i < 5; i++) {
    if (((tick + i * 5) % 8) < 3) {
      int16_t cx = (int16_t)(bg_hash(i, 40) % area.size.w) + area.origin.x;
      int16_t cy = (int16_t)(bg_hash(i, 41) % (area.size.h / 2)) + area.origin.y;
      graphics_draw_pixel(ctx, GPoint(cx, cy));
    }
  }
  // Occasional water drip — a single vertical streak in a different column each tick.
  if ((tick % 6) == 0) {
    int16_t dx = (int16_t)(bg_hash(tick, 42) % area.size.w) + area.origin.x;
    int16_t dy = area.origin.y + 4;
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
    graphics_draw_line(ctx, GPoint(dx, dy), GPoint(dx, dy + 6));
  }
}

static void effects_storm(GContext *ctx, GRect area, uint8_t tick) {
  // Dense diagonal rain streaks drifting right-to-left (wind from the right).
  // Subtract tick from x so drops move leftward over time; each streak
  // angles left (rx - 2) to match the wind direction.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
  graphics_context_set_stroke_width(ctx, 1);
  for (uint8_t i = 0; i < 30; i++) {
    int16_t rx = (int16_t)(((uint32_t)bg_hash(i, 50) + (uint32_t)area.size.w * 256
                            - (uint32_t)(tick * 3)) % area.size.w) + area.origin.x;
    int16_t ry = (int16_t)((bg_hash(i, 51) + tick * 6) % area.size.h) + area.origin.y;
    graphics_draw_line(ctx, GPoint(rx, ry), GPoint(rx - 2, ry + 5));
  }

  // Lightning — strikes every ~25 ticks, visible for 2 ticks.
  // The bolt position varies per cycle so it doesn't always hit the same spot.
  uint8_t cycle = tick % 25;
  if (cycle < 2) {
    // Bolt x position varies by which cycle we're in (tick / 25 as seed)
    uint16_t bolt_seed = bg_hash((uint16_t)(tick / 25), 55);
    int16_t bx = area.origin.x + 20 + (int16_t)(bolt_seed % (area.size.w - 40));
    int16_t by = area.origin.y;

    // Draw the bolt as a jagged multi-segment line from top downward
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 1);

    int16_t x = bx, y = by;
    for (int seg = 0; seg < 5; seg++) {
      int16_t dx = (int16_t)((bg_hash(bolt_seed + seg, 56) % 11) - 5);  // -5..+5
      int16_t dy = (int16_t)(8 + bg_hash(bolt_seed + seg, 57) % 6);     // 8..13
      int16_t nx = x + dx, ny = y + dy;
      graphics_draw_line(ctx, GPoint(x, y), GPoint(nx, ny));
      x = nx; y = ny;
    }

    // On the first tick of the strike, flash a bright highlight around the bolt origin
    if (cycle == 0) {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorWhite));
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
