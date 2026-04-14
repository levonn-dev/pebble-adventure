#include <pebble.h>
#include "game_state.h"
#include "screens.h"

static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
  screens_on_worker_message(type, message);
}

static void init(void) {
  srand(time(NULL));

  // Subscribe to worker messages before pushing any window
  app_worker_message_subscribe(worker_message_handler);

  Pet pet;
  if (!pet_load(&pet)) {
    // New game — go straight to creation
    screens_push_creation();
    return;
  }

  screens_push_main();

  // If adventure completed in background, push results on top of main
  Adventure adv;
  if (adventure_load(&adv) && adventure_is_complete(&adv)) {
    screens_push_results(adv.total_xp_earned, adv.encounters_total);
  }

  if (!app_worker_is_running()) {
    app_worker_launch();
  }
}

static void deinit(void) {
  app_worker_message_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
