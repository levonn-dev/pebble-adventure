#include "screens.h"
#include "game_state.h"
#include "stats.h"
#include "ui.h"
#include <pebble.h>

void screens_push_creation(void)                                  { /* Task 9  */ }
void screens_push_main(void)                                      { /* Task 10 */ }
void screens_push_adventure(void)                                 { /* Task 11 */ }
void screens_push_results(uint32_t xp, uint8_t enc)               { (void)xp; (void)enc; /* Task 12 */ }
void screens_push_levelup(Pet *pet)                               { (void)pet; /* Task 12 */ }
void screens_on_worker_message(uint16_t t, AppWorkerMessage *msg) { (void)t; (void)msg; /* Task 11 */ }
