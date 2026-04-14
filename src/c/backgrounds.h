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

// Draw biome-specific procedural overlays on top of the already-drawn
// background bitmap. Must be called AFTER backgrounds_draw() and BEFORE
// the fox is drawn, so the fox appears in front of effects.
//
// `effect_tick` is a counter advanced by the animation timer; effects
// use it for periodic/random behavior (lightning timing, particle
// positions, etc.). `scroll_offset` matches what was passed to
// backgrounds_draw() — effects may use it to sync with the background.
void backgrounds_draw_effects(GContext *ctx, GRect area, uint8_t biome,
                              uint16_t scroll_offset, uint8_t effect_tick);
