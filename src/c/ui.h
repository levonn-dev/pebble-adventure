/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <pebble.h>

// Draw time (top-left) and battery % (top-right) using GOTHIC_14_BOLD.
// Always call this first in any update_proc so the status bar is visible.
void ui_draw_status_bar(GContext *ctx, GRect bounds);

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

// Draw large fox sprite centered at 'center'. Uses large sprite set.
void ui_draw_fox_large(GContext *ctx, GPoint center, FoxState state, uint8_t frame);

// Draw a menu row with optional selection highlight.
// If selected: inverted colors (white bg on B&W, dark gray on color).
// Adds 2px descender padding below text_h for chars like g, j, y.
// Returns the text color to use (so caller can draw text after).
GColor ui_draw_menu_row(GContext *ctx, int16_t y, int16_t w, int16_t text_h, bool selected);

// Keep backlight on for 30 seconds. Call on any user interaction.
void ui_keep_backlight(void);

// Vibration helpers — respect the user's vibration setting.
bool ui_vibration_enabled(void);
void ui_set_vibration(bool enabled);
void ui_vibe_short(void);
void ui_vibe_double(void);
void ui_vibe_long(void);

// Auto-Adventure setting - reads PERSIST_KEY_AUTO_ADVENTURE, defaults false.
bool ui_auto_adventure_enabled(void);
void ui_set_auto_adventure(bool enabled);
