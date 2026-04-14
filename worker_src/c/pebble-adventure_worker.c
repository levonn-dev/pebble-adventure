#include <pebble_worker.h>

// ---------------------------------------------------------------------------
// Struct copies — MUST match src/c/shared_types.h exactly.
// If you change shared_types.h, update these too.
// ---------------------------------------------------------------------------
#define MAX_SEGMENTS  8
#define PERSIST_KEY_PET           1
#define PERSIST_KEY_ADVENTURE     2
#define PERSIST_KEY_WORKER_STEPS  3
#define PERSIST_KEY_PENDING_ENCOUNTER 4
#define NUM_ENCOUNTERS 5

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

typedef enum {
  WORKER_MSG_STEPS_UPDATE   = 0,
  WORKER_MSG_SEGMENT_DONE   = 1,
  WORKER_MSG_ADVENTURE_DONE = 2,
  WORKER_MSG_ENCOUNTER      = 3,
} WorkerMsgType;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t s_last_steps = 0;
static uint32_t s_steps_in_window = 0;  // steps since last encounter check

// ---------------------------------------------------------------------------
// Inline progress logic — mirrors adventure_apply_steps in game_state.c.
// Biome difficulty = biome index + 1 (PLAINS=1 ... STORM=6).
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
  if (event != HealthEventMovementUpdate && event != HealthEventSignificantUpdate) return;

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
    app_worker_send_message(WORKER_MSG_ADVENTURE_DONE, &msg);
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
