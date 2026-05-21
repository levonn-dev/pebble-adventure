/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <pebble.h>
#include "../shared_types.h"

// Shared state -- written by owning screen, read by screen_worker.c
extern Window    *s_cr_window;

// Shared across creation and levelup screens
extern const char *s_stat_names[NUM_STATS];

// Called by screen_worker.c to reload adventure state and redraw
void adv_screen_refresh(void);

// Called by screen_worker.c when encounter message arrives
void adv_resolve_pending_encounter(void);

// Called by screen_worker.c when WORKER_MSG_AUTO_DONE arrives, and by app
// init() when a pending AutoSummary exists. Reads & clears the summary,
// queues a single popup, reloads pet and adventure state.
void adv_resolve_auto_done(void);

// Queue a popup message to show on the adventure screen.
// If the adventure screen isn't visible, it shows when it next loads.
void adv_queue_popup(const char *title, const char *detail, bool won);
