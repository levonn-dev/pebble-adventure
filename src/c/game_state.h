/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <pebble.h>
#include "shared_types.h"
#include "stats.h"

// --- Biome config ---
typedef struct {
  const char *name;
  uint8_t     primary_stat;    // STAT_IDX_* constant
  uint8_t     secondary_stat;  // STAT_IDX_* constant
  uint32_t    base_steps;
  uint8_t     difficulty;      // 1-6
} BiomeConfig;

const BiomeConfig *biome_get_config(BiomeType biome);

// --- Pet ---
// Initialise a new pet with default stats and 20 upgrade points.
void pet_init_new(Pet *pet, const char *name);
bool pet_load(Pet *pet);
void pet_save(const Pet *pet);

// --- Adventure ---
// Generate a new adventure (5-8 segments, weighted easy-to-hard).
// Pre-computes segment_length from pet's current stats.
void adventure_init(Adventure *adv, const Pet *pet);
bool adventure_load(Adventure *adv);
void adventure_save(const Adventure *adv);

// Apply raw step count to current segment.
// Advances current_segment when a segment completes.
// Sets adv->active = false and accumulates total_xp_earned when all done.
void adventure_apply_steps(Adventure *adv, uint32_t steps);

// Current segment completion as 0-100.
uint8_t adventure_segment_progress_pct(const Adventure *adv);

// True when all segments are done (active == false and num_segments > 0).
bool adventure_is_complete(const Adventure *adv);

void adventure_add_progress_pct(Adventure *adv, int8_t percent);

// Compute the pet's step multiplier for a biome (percentage, e.g. 100 = 1x).
// Formula: 100 + floor(primary*5/10) + floor(secondary*2/10)
uint32_t biome_step_multiplier(const BiomeConfig *cfg, const Pet *pet);
