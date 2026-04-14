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
