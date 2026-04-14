#include <pebble.h>
#include "game_state.h"
#include "events.h"
#include "screens.h"
#include "screens/screens_internal.h"
#include "sprites.h"
#include "ui.h"

static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
  screens_on_worker_message(type, message);
}

static void init(void) {
  srand(time(NULL));
  sprites_init();

  // Subscribe to worker messages before pushing any window
  app_worker_message_subscribe(worker_message_handler);

  Pet pet;
  if (!pet_load(&pet)) {
    // New game — go straight to creation
    screens_push_creation();
    return;
  }

  screens_push_adventure();

  // If adventure completed in background, push results on top of main
  Adventure adv;
  if (adventure_load(&adv) && adventure_is_complete(&adv)) {
    screens_push_results(adv.total_xp_earned, adv.encounters_total);
  }

  // Resolve any encounter triggered while app was closed
  if (persist_exists(PERSIST_KEY_PENDING_ENCOUNTER)) {
    int32_t enc_id = persist_read_int(PERSIST_KEY_PENDING_ENCOUNTER);
    if (enc_id >= 0 && enc_id < NUM_ENCOUNTERS) {
      Adventure enc_adv;
      if (adventure_load(&enc_adv) && enc_adv.active) {
        EncounterResult result = encounter_resolve((uint8_t)enc_id, &pet);
        encounter_apply(&result, &enc_adv);
        adventure_save(&enc_adv);
        // Queue popup so user sees what happened
        char detail[20];
        encounter_format_detail(&result, detail, sizeof(detail));
        adv_queue_popup(result.encounter_name, detail, result.won);
        ui_vibe_short();
      }
    }
    persist_delete(PERSIST_KEY_PENDING_ENCOUNTER);
  }

  if (!app_worker_is_running()) {
    app_worker_launch();
  }
}

static void deinit(void) {
  app_worker_message_unsubscribe();
  sprites_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
