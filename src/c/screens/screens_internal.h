#pragma once
#include <pebble.h>
#include "../shared_types.h"

// Shared state -- written by owning screen, read by screen_worker.c
extern Layer     *s_adv_layer;
extern Layer     *s_main_layer;
extern Adventure  s_adv_current;
extern bool       s_main_has_active_adv;
extern Window    *s_main_window;
extern Window    *s_cr_window;

// Shared across creation and levelup screens
extern const char *s_stat_names[NUM_STATS];

// Called by screen_worker.c when encounter message arrives
void adv_resolve_pending_encounter(void);
