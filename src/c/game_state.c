#include "game_state.h"
#include <pebble.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Biome configuration table
// ---------------------------------------------------------------------------
static const BiomeConfig s_biomes[NUM_BIOMES] = {
  { "Plains",   STAT_IDX_AGI, STAT_IDX_STR, 500,  1 },
  { "Forest",   STAT_IDX_DEX, STAT_IDX_LUK, 600,  2 },
  { "Water",    STAT_IDX_STR, STAT_IDX_VIT, 700,  3 },
  { "Mountain", STAT_IDX_STR, STAT_IDX_AGI, 800,  4 },
  { "Cave",     STAT_IDX_INT, STAT_IDX_LUK, 900,  5 },
  { "Storm",    STAT_IDX_VIT, STAT_IDX_STR, 1000, 6 },
};

const BiomeConfig *biome_get_config(BiomeType biome) {
  if ((uint8_t)biome >= NUM_BIOMES) return &s_biomes[0];
  return &s_biomes[(uint8_t)biome];
}

uint32_t biome_step_multiplier(const BiomeConfig *cfg, const Pet *pet) {
  uint16_t primary   = pet_get_stat(pet, cfg->primary_stat);
  uint16_t secondary = pet_get_stat(pet, cfg->secondary_stat);
  return 100 + (primary * 5 / 10) + (secondary * 2 / 10);
}

// ---------------------------------------------------------------------------
// Pet
// ---------------------------------------------------------------------------
void pet_init_new(Pet *pet, const char *name) {
  memset(pet, 0, sizeof(Pet));
  strncpy(pet->name, name, sizeof(pet->name) - 1);
  pet->level          = 1;
  pet->xp             = 0;
  pet->xp_next_level  = stats_xp_to_next_level(1);
  pet->upgrade_points = 20;
  pet->str            = 1;
  pet->dex            = 1;
  pet->agi            = 1;
  pet->vit            = 1;
  pet->intel          = 1;
  pet->luk            = 1;
}

bool pet_load(Pet *pet) {
  if (!persist_exists(PERSIST_KEY_PET)) return false;
  persist_read_data(PERSIST_KEY_PET, pet, sizeof(Pet));
  return true;
}

void pet_save(const Pet *pet) {
  persist_write_data(PERSIST_KEY_PET, pet, sizeof(Pet));
}

// ---------------------------------------------------------------------------
// Adventure
// ---------------------------------------------------------------------------
void adventure_init(Adventure *adv, const Pet *pet) {
  memset(adv, 0, sizeof(Adventure));
  adv->active = true;

  // 5-8 segments
  adv->num_segments = 5 + (uint8_t)(rand() % 4);

  // Biomes weighted loosely easy-to-hard:
  // max allowed biome index increases as segment index increases
  for (uint8_t i = 0; i < adv->num_segments; i++) {
    uint8_t max_biome = 1 + (i * NUM_BIOMES / adv->num_segments);
    if (max_biome > NUM_BIOMES) max_biome = NUM_BIOMES;
    adv->segments[i] = (uint8_t)(rand() % max_biome);
  }

  // Pre-compute effective step length per segment from current stats.
  // multiplier = 100 + floor(primary*5/10) + floor(secondary*2/10)
  // segment_length = ceil(base_steps * 100 / multiplier)
  for (uint8_t i = 0; i < adv->num_segments; i++) {
    const BiomeConfig *cfg = biome_get_config((BiomeType)adv->segments[i]);
    uint32_t mult = biome_step_multiplier(cfg, pet);
    // Ceiling division
    adv->segment_length[i] = (cfg->base_steps * 100 + mult - 1) / mult;
  }
}

bool adventure_load(Adventure *adv) {
  if (!persist_exists(PERSIST_KEY_ADVENTURE)) return false;
  persist_read_data(PERSIST_KEY_ADVENTURE, adv, sizeof(Adventure));
  return true;
}

void adventure_save(const Adventure *adv) {
  persist_write_data(PERSIST_KEY_ADVENTURE, adv, sizeof(Adventure));
}

void adventure_apply_steps(Adventure *adv, uint32_t steps) {
  uint32_t remaining = steps;
  while (remaining > 0 && adv->current_segment < adv->num_segments) {
    uint32_t seg_remaining = adv->segment_length[adv->current_segment]
                           - adv->segment_progress[adv->current_segment];
    if (remaining >= seg_remaining) {
      adv->segment_progress[adv->current_segment] =
        adv->segment_length[adv->current_segment];
      remaining -= seg_remaining;
      // Accumulate XP: (segment_index * 50) + (biome_difficulty * 30)
      uint8_t diff = biome_get_config((BiomeType)adv->segments[adv->current_segment])->difficulty;
      adv->total_xp_earned += (uint32_t)(adv->current_segment * 50) + (uint32_t)(diff * 30);
      adv->current_segment++;
    } else {
      adv->segment_progress[adv->current_segment] += remaining;
      remaining = 0;
    }
  }
  if (adv->current_segment >= adv->num_segments) {
    adv->active = false;
  }
}

uint8_t adventure_segment_progress_pct(const Adventure *adv) {
  if (adv->current_segment >= adv->num_segments) return 100;
  uint32_t length   = adv->segment_length[adv->current_segment];
  if (length == 0) return 100;
  uint32_t progress = adv->segment_progress[adv->current_segment];
  return (uint8_t)((progress * 100) / length);
}

bool adventure_is_complete(const Adventure *adv) {
  return !adv->active && adv->num_segments > 0;
}

void adventure_add_progress_pct(Adventure *adv, int8_t percent) {
  if (!adv->active || adv->current_segment >= adv->num_segments) return;
  uint8_t seg = adv->current_segment;
  int32_t change = (int32_t)adv->segment_length[seg] * percent / 100;
  int32_t new_val = (int32_t)adv->segment_progress[seg] + change;
  if (new_val < 0) new_val = 0;
  if ((uint32_t)new_val >= adv->segment_length[seg]) {
    new_val = (int32_t)(adv->segment_length[seg] - 1);
  }
  adv->segment_progress[seg] = (uint32_t)new_val;
}
