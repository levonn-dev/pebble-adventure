#include "events.h"
#include "game_state.h"
#include "stats.h"
#include <stdlib.h>

static const EncounterDef s_encounters[NUM_ENCOUNTERS] = {
  { "Wild Animal",     STAT_IDX_STR, STAT_IDX_AGI, true,  10,  -5,  0,  0 },
  { "Slippery Ground", STAT_IDX_AGI, STAT_IDX_DEX, true,   0,  -8,  0,  0 },
  { "Hidden Shortcut", STAT_IDX_INT, STAT_IDX_LUK, true,  15,   0,  0,  0 },
  { "Stranger",        STAT_IDX_INT, 0xFF,         false,  0,   0, 20,  0 },
  { "Lucky Find",      STAT_IDX_LUK, 0xFF,         false,  0,   0, 30,  0 },
};

const EncounterDef *encounter_get_def(uint8_t id) {
  if (id >= NUM_ENCOUNTERS) return &s_encounters[0];
  return &s_encounters[id];
}

EncounterResult encounter_resolve(uint8_t encounter_id, const Pet *pet) {
  const EncounterDef *def = encounter_get_def(encounter_id);

  uint16_t stat_val;
  if (def->stat2 == 0xFF) {
    stat_val = pet_get_stat(pet, def->stat1);
  } else if (def->use_higher) {
    uint16_t a = pet_get_stat(pet, def->stat1);
    uint16_t b = pet_get_stat(pet, def->stat2);
    stat_val = (a > b) ? a : b;
  } else {
    stat_val = pet_get_stat(pet, def->stat1);
  }

  uint16_t threshold = stat_val / 2;
  if (threshold > 80) threshold = 80;
  uint8_t roll = (uint8_t)(rand() % 100);
  bool won = (roll < threshold);

  EncounterResult result;
  result.won = won;
  result.encounter_name = def->name;
  result.progress_change = won ? def->win_progress : def->lose_progress;
  result.bonus_xp = won ? def->win_bonus_xp : def->lose_bonus_xp;
  return result;
}

void encounter_apply(EncounterResult *result, Adventure *adv) {
  if (result->progress_change != 0) {
    adventure_add_progress_pct(adv, result->progress_change);
  }
  if (result->bonus_xp > 0) {
    adv->total_xp_earned += result->bonus_xp;
  }
}

void encounter_format_detail(const EncounterResult *result, char *buf, size_t buf_size) {
  if (result->progress_change != 0) {
    snprintf(buf, buf_size, "%s%d%% progress",
             result->progress_change > 0 ? "+" : "", (int)result->progress_change);
  } else if (result->bonus_xp > 0) {
    snprintf(buf, buf_size, "+%lu XP", (unsigned long)result->bonus_xp);
  } else {
    snprintf(buf, buf_size, "No effect");
  }
}
