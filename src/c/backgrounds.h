#pragma once
#include <pebble.h>

// Load all biome background bitmaps. Call once in app init.
void backgrounds_init(void);

// Free all cached background bitmaps. Call in app deinit.
void backgrounds_deinit(void);

// Draw the biome's ping-pong scrolling background within `area`.
// `biome` is a BiomeType value (0..5) — see shared_types.h.
// `scroll_offset` is a monotonically-advancing counter; the function
// converts it into a ping-pong window position over [0, imgW - areaW]
// so the visible window oscillates instead of wrapping. This avoids
// any need for seamlessly-tileable edges in the source image.
// If the bitmap is not loaded, fills `area` with a solid fallback color.
void backgrounds_draw(GContext *ctx, GRect area, uint8_t biome, uint16_t scroll_offset);
