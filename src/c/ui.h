#pragma once
#include <pebble.h>

// Draw "HH:MM" in top-left using GOTHIC_18_BOLD. Always call this first in
// any update_proc so time is always visible.
void ui_draw_time(GContext *ctx, GRect bounds);

// Draw a horizontal progress bar in rect. Left fills proportionally.
// Color build: fill = GColorGreen, empty = GColorDarkGray.
// B&W build:   fill = GColorWhite, empty = GColorBlack, outline = GColorWhite.
void ui_draw_progress_bar(GContext *ctx, GRect rect, uint32_t value, uint32_t max_val);

// Draw battery percentage in top-right corner of bounds.
void ui_draw_battery(GContext *ctx, GRect bounds);

// Fox sprite states
typedef enum {
  FOX_IDLE,    // Main screen: standing, ears twitch (2 frames)
  FOX_WALK,    // Adventure: leg cycle (4 frames)
  FOX_HAPPY,   // Encounter win, level-up: tail up, bounce
  FOX_SAD,     // Encounter loss: ears drooped
  FOX_DIG,     // Treasure hunt: paw digging (2 frames)
} FoxState;

// Draw fox sprite centered at 'center' in the given state.
void ui_draw_fox(GContext *ctx, GPoint center, FoxState state, uint8_t frame);

// Draw biome-specific background within the given area.
// biome: BiomeType value (0-5). frame: animation frame for moving elements.
void ui_draw_biome_bg(GContext *ctx, GRect area, uint8_t biome, uint8_t frame);
