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

// Draw placeholder fox using basic shapes centered at 'center'.
// frame 0-3 drives a simple tail-wag animation.
// Color build: orange body. B&W build: white body.
void ui_draw_fox_placeholder(GContext *ctx, GPoint center, uint8_t frame);
