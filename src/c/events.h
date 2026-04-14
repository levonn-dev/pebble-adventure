#pragma once
#include <pebble.h>
#include "shared_types.h"

typedef struct {
  const char *name;
  uint8_t stat1;
  uint8_t stat2;          // 0xFF if single-stat
  bool use_higher;        // true = use higher of stat1/stat2
  int8_t win_progress;    // segment progress % change on win
  int8_t lose_progress;   // segment progress % change on lose
  uint32_t win_bonus_xp;
  uint32_t lose_bonus_xp;
} EncounterDef;

typedef struct {
  bool won;
  const char *encounter_name;
  int8_t progress_change;
  uint32_t bonus_xp;
} EncounterResult;

typedef enum {
  ENCOUNTER_STAT_CHECK,
  ENCOUNTER_BATTLE,
  ENCOUNTER_NPC,
} EncounterType;

const EncounterDef *encounter_get_def(uint8_t id);
EncounterResult encounter_resolve(uint8_t encounter_id, const Pet *pet);
void encounter_apply(EncounterResult *result, Adventure *adv);
