#include <pebble.h>
#include "stats.h"

int main(void) {
  // Temporary math verification — remove in Task 8
  APP_LOG(APP_LOG_LEVEL_INFO, "XP L1->2:  %lu (expect 150)", (uint32_t)stats_xp_to_next_level(1));
  APP_LOG(APP_LOG_LEVEL_INFO, "XP L10->11: %lu (expect 844)", (uint32_t)stats_xp_to_next_level(10));
  APP_LOG(APP_LOG_LEVEL_INFO, "Cost @1:   %d (expect 1)",  (int)stats_upgrade_cost(1));
  APP_LOG(APP_LOG_LEVEL_INFO, "Cost @100: %d (expect 2)",  (int)stats_upgrade_cost(100));
  APP_LOG(APP_LOG_LEVEL_INFO, "Cost @999: %d (expect 10)", (int)stats_upgrade_cost(999));

  app_event_loop();
  return 0;
}
