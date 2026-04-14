#include "../screens.h"
#include "screens_internal.h"
#include "../game_state.h"
#include <pebble.h>

void screens_on_worker_message(uint16_t type, AppWorkerMessage *message) {
  (void)message;
  adventure_load(&s_adv_current);
  s_main_has_active_adv = s_adv_current.active;
  if (s_adv_layer)  layer_mark_dirty(s_adv_layer);
  if (s_main_layer) layer_mark_dirty(s_main_layer);

  if (type == WORKER_MSG_ENCOUNTER) {
    adv_resolve_pending_encounter();
  } else if (type == WORKER_MSG_ADVENTURE_DONE) {
    vibes_short_pulse();
  }
}
