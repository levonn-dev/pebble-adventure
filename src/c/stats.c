#include "stats.h"
#include <math.h>
#include <pebble.h>

uint32_t stats_xp_to_next_level(uint16_t level) {
  return (uint32_t)(150.0f * powf((float)level, 1.4f));
}

uint16_t stats_upgrade_cost(uint16_t current_val) {
  return (uint16_t)(current_val / 100) + 1;
}

uint8_t stats_apply_xp(Pet *pet, uint32_t xp_gained) {
  uint8_t levels_gained = 0;
  pet->xp += xp_gained;
  while (pet->xp >= pet->xp_next_level) {
    pet->xp -= pet->xp_next_level;
    pet->level++;
    uint16_t pts = (uint16_t)(pet->level / 5) + 2;
    pet->upgrade_points += pts;
    pet->xp_next_level = stats_xp_to_next_level(pet->level);
    levels_gained++;
    APP_LOG(APP_LOG_LEVEL_INFO, "Level up -> %d! +%d pts (total unspent: %d)",
            (int)pet->level, (int)pts, (int)pet->upgrade_points);
  }
  return levels_gained;
}

bool stats_raise_stat(uint16_t *stat, uint16_t *upgrade_points) {
  if (*stat >= MAX_STAT) return false;
  uint16_t cost = stats_upgrade_cost(*stat);
  if (*upgrade_points < cost) return false;
  *upgrade_points -= cost;
  (*stat)++;
  return true;
}

bool stats_lower_stat(uint16_t *stat, uint16_t *upgrade_points) {
  if (*stat <= 1) return false;
  uint16_t refund = stats_upgrade_cost(*stat - 1);
  *upgrade_points += refund;
  (*stat)--;
  return true;
}

uint16_t pet_get_stat(const Pet *pet, uint8_t index) {
  switch (index) {
    case STAT_IDX_STR: return pet->str;
    case STAT_IDX_DEX: return pet->dex;
    case STAT_IDX_AGI: return pet->agi;
    case STAT_IDX_VIT: return pet->vit;
    case STAT_IDX_INT: return pet->intel;
    case STAT_IDX_LUK: return pet->luk;
    default: return 0;
  }
}

void pet_set_stat(Pet *pet, uint8_t index, uint16_t value) {
  switch (index) {
    case STAT_IDX_STR: pet->str   = value; break;
    case STAT_IDX_DEX: pet->dex   = value; break;
    case STAT_IDX_AGI: pet->agi   = value; break;
    case STAT_IDX_VIT: pet->vit   = value; break;
    case STAT_IDX_INT: pet->intel = value; break;
    case STAT_IDX_LUK: pet->luk   = value; break;
  }
}
