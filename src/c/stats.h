#pragma once
#include "shared_types.h"

// XP required to advance from level N to N+1.
// Formula: floor(150 * N^1.4)
// Spot checks: level 1 -> 150, level 10 -> 844, level 100 -> 5179
uint32_t stats_xp_to_next_level(uint16_t level);

// Upgrade points needed to raise a stat from current_val to current_val+1.
// Formula: floor(current_val / 100) + 1
// Stat 1-99: cost 1. Stat 100-199: cost 2. ... Stat 900-999: cost 10.
uint16_t stats_upgrade_cost(uint16_t current_val);

// Add xp_gained to pet->xp, trigger level-ups.
// Each level-up: pet->level++, pet->upgrade_points += floor(level/5)+2,
//                pet->xp_next_level = stats_xp_to_next_level(new_level).
// Returns number of levels gained.
uint8_t stats_apply_xp(Pet *pet, uint32_t xp_gained);

// Raise stat by 1, deducting the appropriate upgrade cost from *upgrade_points.
// Returns false if stat >= MAX_STAT or points insufficient.
bool stats_raise_stat(uint16_t *stat, uint16_t *upgrade_points);

// Lower stat by 1, refunding the cost of the raise being undone.
// Returns false if stat <= 1.
bool stats_lower_stat(uint16_t *stat, uint16_t *upgrade_points);

// Get / set a stat by STAT_IDX_* index.
uint16_t pet_get_stat(const Pet *pet, uint8_t index);
void     pet_set_stat(Pet *pet, uint8_t index, uint16_t value);
