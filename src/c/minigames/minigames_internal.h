/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <pebble.h>

// Push functions for individual mini-games (called from menu)
void minigame_push_dodge(void);
void minigame_push_catch(void);
void minigame_push_riddle(void);
void minigame_push_treasure(void);
