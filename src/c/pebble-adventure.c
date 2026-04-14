#include <pebble.h>
#include "stats.h"
#include "game_state.h"

int main(void) {
  // Temporary math verification — remove in Task 8
  APP_LOG(APP_LOG_LEVEL_INFO, "XP L1->2:  %lu (expect 150)", (uint32_t)stats_xp_to_next_level(1));
  APP_LOG(APP_LOG_LEVEL_INFO, "XP L10->11: %lu (expect 844)", (uint32_t)stats_xp_to_next_level(10));
  APP_LOG(APP_LOG_LEVEL_INFO, "Cost @1:   %d (expect 1)",  (int)stats_upgrade_cost(1));
  APP_LOG(APP_LOG_LEVEL_INFO, "Cost @100: %d (expect 2)",  (int)stats_upgrade_cost(100));
  APP_LOG(APP_LOG_LEVEL_INFO, "Cost @999: %d (expect 10)", (int)stats_upgrade_cost(999));

  // Adventure generation test
  Pet tp;
  pet_init_new(&tp, "Tester");
  tp.str = 11; tp.vit = 11;
  Adventure ta;
  adventure_init(&ta, &tp);
  APP_LOG(APP_LOG_LEVEL_INFO, "Adventure: %d segs", (int)ta.num_segments);
  for (int i = 0; i < ta.num_segments; i++) {
    APP_LOG(APP_LOG_LEVEL_INFO, "  seg%d biome=%d len=%lu",
            i, (int)ta.segments[i], ta.segment_length[i]);
  }
  // Simulate 500 steps
  adventure_apply_steps(&ta, 500);
  APP_LOG(APP_LOG_LEVEL_INFO, "After 500 steps: seg=%d xp=%lu",
          (int)ta.current_segment, ta.total_xp_earned);

  app_event_loop();
  return 0;
}
