#pragma once
#include <pebble.h>

// Initialize the backgrounds module. Does NOT load any bitmaps — biome
// bitmaps are loaded lazily on first draw to keep memory footprint low
// (a single 400x144 color bitmap is ~57KB, and the Pebble app heap is
// too small to hold all six biomes at once).
void backgrounds_init(void);

// Free the currently-loaded background bitmap (if any). Call in app deinit.
void backgrounds_deinit(void);

// Draw the biome's background within `area`.
// `biome` is a BiomeType value (0..5) — see shared_types.h.
//
// Lazy-loads the biome's bitmap on demand: if `biome` differs from the
// currently-cached biome, the previous bitmap is freed and the new one
// is loaded before drawing. Only one biome is kept in memory at a time.
//
// If the bitmap fails to load, fills `area` with a solid fallback color.
void backgrounds_draw(GContext *ctx, GRect area, uint8_t biome);

// Return the ground surface y-coordinate within the given area for the
// specified biome. The fox's paws should align to this y value. Each biome
// has a different ground line based on its background art. Values are
// tunable — adjust the per-biome constants in backgrounds.c if needed.
int16_t backgrounds_ground_y(GRect area, uint8_t biome);

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
