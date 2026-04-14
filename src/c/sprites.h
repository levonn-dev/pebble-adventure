#pragma once
#include <pebble.h>
#include "ui.h"

// Initialize sprite bitmaps from PNG resources. Call once in init().
void sprites_init(void);

// Free all sprite bitmaps. Call once in deinit().
void sprites_deinit(void);

// Get the GBitmap for a fox state + frame. Returns NULL if not loaded.
// Idle: frames 0-1, Walk: frames 0-3, Happy: frame 0, Sad: frame 0, Dig: frames 0-1.
GBitmap *sprites_get_fox(FoxState state, uint8_t frame);

// Large sprites — loaded/unloaded on demand by adventure screen
void sprites_load_large(void);
void sprites_unload_large(void);
GBitmap *sprites_get_fox_large(FoxState state, uint8_t frame);
