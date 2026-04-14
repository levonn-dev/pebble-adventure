#pragma once
#include <pebble.h>
#include "shared_types.h"

typedef enum {
  WORKER_MSG_STEPS_UPDATE   = 0,
  WORKER_MSG_SEGMENT_DONE   = 1,
  WORKER_MSG_ADVENTURE_DONE = 2,
  WORKER_MSG_ENCOUNTER      = 3,
} WorkerMsgType;

void screens_push_creation(void);
void screens_push_main(void);
void screens_push_adventure(void);
void screens_push_results(uint32_t xp_earned, uint8_t encounters);
void screens_push_levelup(Pet *pet);
void screens_push_stats(void);

// Called from main.c when a worker message arrives while the app is open.
void screens_on_worker_message(uint16_t type, AppWorkerMessage *message);
