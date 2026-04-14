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

void backgrounds_draw(GContext *ctx, GRect area, uint8_t biome, uint16_t scroll_offset) {
  ensure_loaded(biome);
  if (!s_current_bg) {
    draw_fallback(ctx, area, biome);
    return;
  }

  GBitmap *bg = s_current_bg;
  GSize img = gbitmap_get_bounds(bg).size;

  // If the image is narrower than the area, nothing to scroll — draw it stretched to fit.
  int32_t range = (int32_t)img.w - (int32_t)area.size.w;
  if (range <= 0) {
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, bg, area);
    return;
  }

  // Ping-pong: map the monotonic scroll_offset counter to an oscillating
  // window x position over [0, range]. The period is 2 * range — during
  // the first half, x ramps 0 → range; during the second half, x ramps
  // range → 0. At the endpoints, motion "reverses direction" smoothly.
  uint32_t period = (uint32_t)range * 2;
  uint32_t pos    = (uint32_t)scroll_offset % period;
  int16_t  x      = (int16_t)(pos <= (uint32_t)range ? pos : period - pos);

  // Extract the visible window as a sub-bitmap view (no pixel copy) and
  // draw it into the area. The sub-bitmap is (areaW × imgH); Pebble will
  // scale vertically if area.size.h differs from img.h.
  int16_t sub_h = img.h < area.size.h ? img.h : area.size.h;
  GBitmap *sub = gbitmap_create_as_sub_bitmap(bg,
    GRect(x, 0, area.size.w, sub_h));
  if (sub) {
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, sub, area);
    gbitmap_destroy(sub);
  } else {
    draw_fallback(ctx, area, biome);
  }
}

// ---------------------------------------------------------------------------
// Procedural effect overlays — drawn on top of the background bitmap.
// ---------------------------------------------------------------------------

static void effects_plains(GContext *ctx, GRect area, uint8_t tick) {
  // Drifting clouds: small white dots that slowly move horizontally across
  // the top of the area. Positions are deterministic per index, offset by tick.
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (uint8_t i = 0; i < 3; i++) {
    uint16_t x_seed = bg_hash(i, 0);
    int16_t  cx = (int16_t)((x_seed + tick * 2) % area.size.w) + area.origin.x;
    int16_t  cy = area.origin.y + 6 + (int16_t)(bg_hash(i, 1) % 10);
    graphics_fill_circle(ctx, GPoint(cx, cy), 2);
  }
  // Pollen/seed particles: tiny specks drifting across the middle band.
  // Skip pollen if the area is too short to contain the middle band.
  if (area.size.h > 40) {
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    for (uint8_t i = 0; i < 5; i++) {
      uint16_t x_seed = bg_hash(i, 2);
      int16_t  px = (int16_t)((x_seed + tick * 3) % area.size.w) + area.origin.x;
      int16_t  py = area.origin.y + 30 + (int16_t)(bg_hash(i, 3) % (area.size.h - 40));
      graphics_draw_pixel(ctx, GPoint(px, py));
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
  // Dense diagonal rain streaks across the full area. Positions are seeded
  // per index and offset by tick so they appear to fall.
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
  graphics_context_set_stroke_width(ctx, 1);
  for (uint8_t i = 0; i < 30; i++) {
    int16_t rx = (int16_t)((bg_hash(i, 50) + tick * 3) % area.size.w) + area.origin.x;
    int16_t ry = (int16_t)((bg_hash(i, 51) + tick * 6) % area.size.h) + area.origin.y;
    graphics_draw_line(ctx, GPoint(rx, ry), GPoint(rx - 2, ry + 5));
  }
  // Periodic lightning flash — every 20 ticks, flash for one tick.
  // Small top-band flash (24px) instead of the entire area so the
  // background and fox remain visible underneath.
  if ((tick % 20) == 0) {
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_fill_rect(ctx,
      GRect(area.origin.x, area.origin.y, area.size.w, 24),
      0, GCornerNone);
    // Zigzag bolt
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 2);
    int16_t lx = area.origin.x + area.size.w / 2;
    int16_t ly = area.origin.y;
    graphics_draw_line(ctx, GPoint(lx,     ly),      GPoint(lx - 4, ly + 10));
    graphics_draw_line(ctx, GPoint(lx - 4, ly + 10), GPoint(lx + 3, ly + 18));
    graphics_draw_line(ctx, GPoint(lx + 3, ly + 18), GPoint(lx - 2, ly + 28));
  }
  // Reset stroke width to default so it doesn't leak to later draws.
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
