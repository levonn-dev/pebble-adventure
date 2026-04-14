#include "backgrounds.h"
#include "shared_types.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// Biome backgrounds — lazy-loaded PNG bitmaps.
//
// Only the currently-active biome's bitmap is kept in memory at any time.
// When backgrounds_draw is called with a different biome, the previous
// bitmap is destroyed and the new one loaded.
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

  APP_LOG(APP_LOG_LEVEL_INFO, "bg: switching %d -> %d, heap_free=%u",
          (int)s_current_biome, (int)biome, (unsigned)heap_bytes_free());

  if (s_current_bg) {
    gbitmap_destroy(s_current_bg);
    s_current_bg = NULL;
  }
  s_current_biome = BG_NONE;

  APP_LOG(APP_LOG_LEVEL_INFO, "bg: after free, heap_free=%u",
          (unsigned)heap_bytes_free());

  uint32_t rid = bg_resource_id(biome);
  if (rid == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "bg: no resource id for biome %d", (int)biome);
    return;
  }

  s_current_bg = gbitmap_create_with_resource(rid);
  if (s_current_bg) {
    s_current_biome = biome;
    APP_LOG(APP_LOG_LEVEL_INFO, "bg: loaded biome %d OK, heap_free=%u",
            (int)biome, (unsigned)heap_bytes_free());
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "bg: FAILED to load biome %d rid=%lu, heap_free=%u",
            (int)biome, (unsigned long)rid, (unsigned)heap_bytes_free());
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
    // Image is taller (or equal) — crop from top, draw full area height.
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
int16_t backgrounds_ground_y(GRect area, uint8_t biome) {
#ifdef PBL_COLOR
  int16_t offset;
  switch ((BiomeType)biome) {
    case BIOME_PLAINS:   offset = 24; break;
    case BIOME_FOREST:   offset = 22; break;
    case BIOME_WATER:    offset = 22; break;
    case BIOME_MOUNTAIN: offset = 20; break;
    case BIOME_CAVE:     offset = 20; break;
    case BIOME_STORM:    offset = 22; break;
    default:             offset = 20; break;
  }
#else
  // Flint: B&W backgrounds, fox should sit near the bottom
  (void)biome;
  int16_t offset = 10;
#endif
  return area.origin.y + area.size.h - offset;
}
