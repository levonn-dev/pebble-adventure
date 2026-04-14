#include "backgrounds.h"
#include "shared_types.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// Biome backgrounds — 6 wide PNGs that ping-pong scroll horizontally to
// simulate the fox's movement during an adventure. Indexed by BiomeType.
// ---------------------------------------------------------------------------
#define BG_COUNT 6

static GBitmap *s_bg[BG_COUNT] = { NULL };

void backgrounds_init(void) {
  s_bg[BIOME_PLAINS]   = gbitmap_create_with_resource(RESOURCE_ID_BG_PLAINS);
  s_bg[BIOME_FOREST]   = gbitmap_create_with_resource(RESOURCE_ID_BG_FOREST);
  s_bg[BIOME_WATER]    = gbitmap_create_with_resource(RESOURCE_ID_BG_WATER);
  s_bg[BIOME_MOUNTAIN] = gbitmap_create_with_resource(RESOURCE_ID_BG_MOUNTAIN);
  s_bg[BIOME_CAVE]     = gbitmap_create_with_resource(RESOURCE_ID_BG_CAVE);
  s_bg[BIOME_STORM]    = gbitmap_create_with_resource(RESOURCE_ID_BG_STORM);
}

void backgrounds_deinit(void) {
  for (int i = 0; i < BG_COUNT; i++) {
    if (s_bg[i]) {
      gbitmap_destroy(s_bg[i]);
      s_bg[i] = NULL;
    }
  }
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
  if (biome >= BG_COUNT || !s_bg[biome]) {
    draw_fallback(ctx, area, biome);
    return;
  }

  GBitmap *bg = s_bg[biome];
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
