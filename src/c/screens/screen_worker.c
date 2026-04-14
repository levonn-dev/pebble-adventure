#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include "../ui.h"
#include <pebble.h>

void screens_on_worker_message(uint16_t type, AppWorkerMessage *message) {
  (void)message;
  adv_screen_refresh();

  if (type == WORKER_MSG_ENCOUNTER) {
    adv_resolve_pending_encounter();
  } else if (type == WORKER_MSG_ADVENTURE_DONE) {
    ui_vibe_long();
  } else if (type == WORKER_MSG_SEGMENT_DONE) {
    ui_vibe_double();
  }
}
