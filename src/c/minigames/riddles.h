/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stdint.h>

#define NUM_RIDDLES    20
#define RIDDLE_ANSWERS 4

typedef struct {
  const char *question;
  const char *answers[RIDDLE_ANSWERS];
  uint8_t correct;
} Riddle;

const Riddle *riddle_get(uint8_t index);
uint8_t riddle_random_index(void);
