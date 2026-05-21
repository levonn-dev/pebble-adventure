/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <pebble_worker.h>

// ---------------------------------------------------------------------------
// Struct copies — MUST match src/c/shared_types.h exactly.
// Workers cannot include app headers, so these are duplicated here.
// COUPLING (must stay in sync with):
//   src/c/shared_types.h   - Pet, Adventure, AutoSummary, PERSIST_KEY_*, STAT_IDX_*
//   src/c/screens.h        - WorkerMsgType enum values
//   src/c/game_state.c     - s_biomes[] table, adventure_init, biome_step_multiplier
//   src/c/stats.c          - stats_apply_xp, stats_xp_to_next_level formulas
// ---------------------------------------------------------------------------
#include <math.h>

#define MAX_SEGMENTS  8
#define NUM_BIOMES    6
#define MAX_STAT      999

#define PERSIST_KEY_PET                1
#define PERSIST_KEY_ADVENTURE          2
#define PERSIST_KEY_WORKER_STEPS       3
#define PERSIST_KEY_PENDING_ENCOUNTER  4
#define PERSIST_KEY_AUTO_ADVENTURE     6
#define PERSIST_KEY_AUTO_SUMMARY       7

#define NUM_ENCOUNTERS 5

#define STAT_IDX_STR  0
#define STAT_IDX_DEX  1
#define STAT_IDX_AGI  2
#define STAT_IDX_VIT  3
#define STAT_IDX_INT  4
#define STAT_IDX_LUK  5

typedef struct {
  char     name[12];
  uint16_t level;
  uint32_t xp;
  uint32_t xp_next_level;
  uint16_t upgrade_points;
  uint16_t str, dex, agi, vit, intel, luk;
} WorkerPet;

typedef struct {
  bool     active;
  uint8_t  num_segments;
  uint8_t  current_segment;
  uint8_t  segments[MAX_SEGMENTS];
  uint32_t segment_length[MAX_SEGMENTS];
  uint32_t segment_progress[MAX_SEGMENTS];
  uint32_t total_xp_earned;
  uint8_t  encounters_total;
} WorkerAdventure;

typedef struct {
  uint8_t  count;
  uint16_t levels;
  uint32_t xp;
} WorkerAutoSummary;

typedef struct {
  uint8_t  primary_stat;
  uint8_t  secondary_stat;
  uint32_t base_steps;
  uint8_t  difficulty;
} WorkerBiomeConfig;

// Values must match s_biomes[] in src/c/game_state.c.
static const WorkerBiomeConfig s_worker_biomes[NUM_BIOMES] = {
  { STAT_IDX_AGI, STAT_IDX_STR, 500,  1 },  // Plains
  { STAT_IDX_DEX, STAT_IDX_LUK, 600,  2 },  // Forest
  { STAT_IDX_STR, STAT_IDX_VIT, 700,  3 },  // Water
  { STAT_IDX_STR, STAT_IDX_AGI, 800,  4 },  // Mountain
  { STAT_IDX_INT, STAT_IDX_LUK, 900,  5 },  // Cave
  { STAT_IDX_VIT, STAT_IDX_STR, 1000, 6 },  // Storm
};

typedef enum {
  WORKER_MSG_STEPS_UPDATE   = 0,
  WORKER_MSG_SEGMENT_DONE   = 1,
  WORKER_MSG_ADVENTURE_DONE = 2,
  WORKER_MSG_ENCOUNTER      = 3,
  WORKER_MSG_AUTO_DONE      = 4,
} WorkerMsgType;

// ---------------------------------------------------------------------------
// Stat & biome helpers (mirror pet_get_stat / biome_step_multiplier).
// ---------------------------------------------------------------------------
static uint16_t worker_pet_get_stat(const WorkerPet *pet, uint8_t index) {
  switch (index) {
    case STAT_IDX_STR: return pet->str;
    case STAT_IDX_DEX: return pet->dex;
    case STAT_IDX_AGI: return pet->agi;
    case STAT_IDX_VIT: return pet->vit;
    case STAT_IDX_INT: return pet->intel;
    case STAT_IDX_LUK: return pet->luk;
    default:           return 0;
  }
}

static uint32_t worker_biome_step_multiplier(const WorkerBiomeConfig *cfg, const WorkerPet *pet) {
  uint16_t primary   = worker_pet_get_stat(pet, cfg->primary_stat);
  uint16_t secondary = worker_pet_get_stat(pet, cfg->secondary_stat);
  return 100 + (primary * 5 / 10) + (secondary * 2 / 10);
}

// Mirror of stats_apply_xp in src/c/stats.c. Returns number of levels gained.
// Formulas (must stay in sync with stats.c):
//   xp_next_level = floor(150 * level^1.4)
//   points_per_levelup = (level / 5) + 2
static uint8_t worker_auto_apply_xp(WorkerPet *pet, uint32_t xp_gained) {
  uint8_t levels_gained = 0;
  pet->xp += xp_gained;
  while (pet->xp >= pet->xp_next_level) {
    pet->xp -= pet->xp_next_level;
    pet->level++;
    uint16_t pts = (uint16_t)(pet->level / 5) + 2;
    pet->upgrade_points += pts;
    pet->xp_next_level = (uint32_t)(150.0f * powf((float)pet->level, 1.4f));
    levels_gained++;
  }
  return levels_gained;
}

// Mirror of adventure_init in src/c/game_state.c.
// 5-8 segments, biomes weighted easy-to-hard (max biome index grows with
// segment index), segment_length pre-computed from the pet's current stats.
static void worker_auto_adventure_init(WorkerAdventure *adv, const WorkerPet *pet) {
  memset(adv, 0, sizeof(WorkerAdventure));
  adv->active = true;
  adv->num_segments = 5 + (uint8_t)(rand() % 4);

  for (uint8_t i = 0; i < adv->num_segments; i++) {
    uint8_t max_biome = 1 + (i * NUM_BIOMES / adv->num_segments);
    if (max_biome > NUM_BIOMES) max_biome = NUM_BIOMES;
    adv->segments[i] = (uint8_t)(rand() % max_biome);
  }

  // segment_length[i] = ceil(base_steps * 100 / multiplier).
  for (uint8_t i = 0; i < adv->num_segments; i++) {
    const WorkerBiomeConfig *cfg = &s_worker_biomes[adv->segments[i]];
    uint32_t mult = worker_biome_step_multiplier(cfg, pet);
    adv->segment_length[i] = (cfg->base_steps * 100 + mult - 1) / mult;
  }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t s_last_steps = 0;
static uint32_t s_steps_in_window = 0;  // steps since last encounter check

// ---------------------------------------------------------------------------
// Inline progress logic — mirrors adventure_apply_steps in game_state.c.
// COUPLING: XP formula assumes difficulty = biome_index + 1 (PLAINS=1 ... STORM=6).
// If biome difficulty values in game_state.c's s_biomes[] ever diverge from
// this pattern, this formula must be updated to match.
// Returns 0=steps only, 1=segment done, 2=adventure done.
// ---------------------------------------------------------------------------
static uint8_t worker_apply_steps(WorkerAdventure *adv, uint32_t steps) {
  uint8_t  result    = 0;
  uint32_t remaining = steps;

  while (remaining > 0 && adv->current_segment < adv->num_segments) {
    uint32_t seg_remaining = adv->segment_length[adv->current_segment]
                           - adv->segment_progress[adv->current_segment];
    if (remaining >= seg_remaining) {
      adv->segment_progress[adv->current_segment] =
        adv->segment_length[adv->current_segment];
      remaining -= seg_remaining;
      uint8_t diff = adv->segments[adv->current_segment] + 1;
      adv->total_xp_earned += (uint32_t)(adv->current_segment * 50) + (uint32_t)(diff * 30);
      adv->current_segment++;
      result = 1;
    } else {
      adv->segment_progress[adv->current_segment] += remaining;
      remaining = 0;
    }
  }
  if (adv->current_segment >= adv->num_segments) {
    adv->active = false;
    result = 2;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Health handler — fires on movement updates
// ---------------------------------------------------------------------------
static void health_handler(HealthEventType event, void *context) {
  (void)context;
  // Accept any health event that might indicate step changes
  uint32_t current = (uint32_t)health_service_sum_today(HealthMetricStepCount);
  if (current == s_last_steps) return;

  // Handle midnight reset: step counter resets to 0 at midnight,
  // so current < s_last_steps means a new day started.
  uint32_t delta;
  if (current < s_last_steps) {
    delta = current;  // All of today's steps are new
  } else {
    delta = current - s_last_steps;
  }
  if (delta == 0) return;
  s_last_steps   = current;
  persist_write_int(PERSIST_KEY_WORKER_STEPS, (int32_t)s_last_steps);

  WorkerAdventure adv;
  if (persist_read_data(PERSIST_KEY_ADVENTURE, &adv, sizeof(WorkerAdventure))
      != (int)sizeof(WorkerAdventure)) return;
  if (!adv.active) return;

  uint8_t outcome = worker_apply_steps(&adv, delta);

  // --- Encounter detection (15% per 100 steps) ---
  if (adv.active) {
    s_steps_in_window += delta;
    while (s_steps_in_window >= 100) {
      s_steps_in_window -= 100;
      if ((rand() % 100) < 15) {
        uint8_t enc_id = (uint8_t)(rand() % NUM_ENCOUNTERS);
        adv.encounters_total++;
        // Save adventure BEFORE sending message to avoid race with app resolution
        persist_write_data(PERSIST_KEY_ADVENTURE, &adv, sizeof(WorkerAdventure));
        persist_write_int(PERSIST_KEY_PENDING_ENCOUNTER, (int32_t)enc_id);
        AppWorkerMessage enc_msg = {
          .data0 = enc_id,
          .data1 = 0
        };
        app_worker_send_message(WORKER_MSG_ENCOUNTER, &enc_msg);
        break;  // at most one encounter per 100-step window
      }
    }
  }

  persist_write_data(PERSIST_KEY_ADVENTURE, &adv, sizeof(WorkerAdventure));

  AppWorkerMessage msg = {
    .data0 = (uint16_t)(delta > 0xFFFF ? 0xFFFF : delta),
    .data1 = 0
  };

  if (outcome == 2) {
    bool auto_mode = persist_exists(PERSIST_KEY_AUTO_ADVENTURE)
                  && persist_read_int(PERSIST_KEY_AUTO_ADVENTURE) != 0;

    WorkerPet pet;
    bool pet_ok = (persist_read_data(PERSIST_KEY_PET, &pet, sizeof(WorkerPet))
                   == (int)sizeof(WorkerPet));

    if (auto_mode && pet_ok) {
      uint32_t earned_xp = adv.total_xp_earned;

      uint8_t levels = worker_auto_apply_xp(&pet, earned_xp);
      persist_write_data(PERSIST_KEY_PET, &pet, sizeof(WorkerPet));

      WorkerAdventure new_adv;
      worker_auto_adventure_init(&new_adv, &pet);
      persist_write_data(PERSIST_KEY_ADVENTURE, &new_adv, sizeof(WorkerAdventure));

      WorkerAutoSummary sum = {0};
      if (persist_exists(PERSIST_KEY_AUTO_SUMMARY)) {
        persist_read_data(PERSIST_KEY_AUTO_SUMMARY, &sum, sizeof(sum));
      }
      sum.count  += 1;
      sum.xp     += earned_xp;
      sum.levels += levels;
      persist_write_data(PERSIST_KEY_AUTO_SUMMARY, &sum, sizeof(sum));

      AppWorkerMessage am = {0};
      app_worker_send_message(WORKER_MSG_AUTO_DONE, &am);
    } else {
      // Off, or pet missing - fall back to legacy behavior.
      app_worker_send_message(WORKER_MSG_ADVENTURE_DONE, &msg);
    }
  } else if (outcome == 1) {
    app_worker_send_message(WORKER_MSG_SEGMENT_DONE, &msg);
  } else if (outcome == 0) {
    app_worker_send_message(WORKER_MSG_STEPS_UPDATE, &msg);
  }
}

// ---------------------------------------------------------------------------
// Worker main
// ---------------------------------------------------------------------------
int main(void) {
  srand(time(NULL));
  if (persist_exists(PERSIST_KEY_WORKER_STEPS)) {
    s_last_steps = (uint32_t)persist_read_int(PERSIST_KEY_WORKER_STEPS);
  }
  health_service_events_subscribe(health_handler, NULL);
  worker_event_loop();
  health_service_events_unsubscribe();
  return 0;
}
