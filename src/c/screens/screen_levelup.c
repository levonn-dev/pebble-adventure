#include "../screens.h"
#include "screens_internal.h"
#include "screen_stat_alloc.h"
#include "../game_state.h"
#include <pebble.h>

// ---------------------------------------------------------------------------
// SCREEN_LEVELUP — delegates to shared stat allocation screen
// ---------------------------------------------------------------------------

static void lu_done(Pet *pet) {
  pet_save(pet);
  // stat alloc pops itself after calling this callback
  screens_push_main();
}

void screens_push_levelup(Pet *pet) {
  screens_push_stat_alloc("LEVEL UP!", pet, lu_done);
}
