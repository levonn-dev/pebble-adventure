#pragma once
#include <pebble.h>
#include "../shared_types.h"

// Callback when user presses Done on stat allocation
typedef void (*StatAllocDoneCallback)(Pet *pet);

// Push a generic stat allocation screen.
// title: header text (e.g. "LEVEL UP!" or "ALLOCATE STATS")
// pet: pet to modify (copied in, saved via callback)
// done_cb: called when user confirms Done
void screens_push_stat_alloc(const char *title, Pet *pet, StatAllocDoneCallback done_cb);
