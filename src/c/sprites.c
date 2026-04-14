#include "sprites.h"
#include <pebble.h>

// Frame counts per state
#define IDLE_FRAMES  2
#define WALK_FRAMES  4
#define HAPPY_FRAMES 1
#define SAD_FRAMES   1
#define DIG_FRAMES   2

// Bitmap storage
static GBitmap *s_fox_idle[IDLE_FRAMES]   = { NULL };
static GBitmap *s_fox_walk[WALK_FRAMES]   = { NULL };
static GBitmap *s_fox_happy[HAPPY_FRAMES] = { NULL };
static GBitmap *s_fox_sad[SAD_FRAMES]     = { NULL };
static GBitmap *s_fox_dig[DIG_FRAMES]     = { NULL };

void sprites_init(void) {
  s_fox_idle[0]  = gbitmap_create_with_resource(RESOURCE_ID_FOX_IDLE_0);
  s_fox_idle[1]  = gbitmap_create_with_resource(RESOURCE_ID_FOX_IDLE_1);

  s_fox_walk[0]  = gbitmap_create_with_resource(RESOURCE_ID_FOX_WALK_0);
  s_fox_walk[1]  = gbitmap_create_with_resource(RESOURCE_ID_FOX_WALK_1);
  s_fox_walk[2]  = gbitmap_create_with_resource(RESOURCE_ID_FOX_WALK_2);
  s_fox_walk[3]  = gbitmap_create_with_resource(RESOURCE_ID_FOX_WALK_3);

  s_fox_happy[0] = gbitmap_create_with_resource(RESOURCE_ID_FOX_HAPPY_0);

  s_fox_sad[0]   = gbitmap_create_with_resource(RESOURCE_ID_FOX_SAD_0);

  s_fox_dig[0]   = gbitmap_create_with_resource(RESOURCE_ID_FOX_DIG_0);
  s_fox_dig[1]   = gbitmap_create_with_resource(RESOURCE_ID_FOX_DIG_1);
}

void sprites_deinit(void) {
  for (int i = 0; i < IDLE_FRAMES; i++)  { if (s_fox_idle[i])  { gbitmap_destroy(s_fox_idle[i]);  s_fox_idle[i] = NULL; } }
  for (int i = 0; i < WALK_FRAMES; i++)  { if (s_fox_walk[i])  { gbitmap_destroy(s_fox_walk[i]);  s_fox_walk[i] = NULL; } }
  for (int i = 0; i < HAPPY_FRAMES; i++) { if (s_fox_happy[i]) { gbitmap_destroy(s_fox_happy[i]); s_fox_happy[i] = NULL; } }
  for (int i = 0; i < SAD_FRAMES; i++)   { if (s_fox_sad[i])   { gbitmap_destroy(s_fox_sad[i]);   s_fox_sad[i] = NULL; } }
  for (int i = 0; i < DIG_FRAMES; i++)   { if (s_fox_dig[i])   { gbitmap_destroy(s_fox_dig[i]);   s_fox_dig[i] = NULL; } }
}

GBitmap *sprites_get_fox(FoxState state, uint8_t frame) {
  switch (state) {
    case FOX_IDLE:  return s_fox_idle[frame % IDLE_FRAMES];
    case FOX_WALK:  return s_fox_walk[frame % WALK_FRAMES];
    case FOX_HAPPY: return s_fox_happy[0];
    case FOX_SAD:   return s_fox_sad[0];
    case FOX_DIG:   return s_fox_dig[frame % DIG_FRAMES];
    default:        return NULL;
  }
}

// ---------------------------------------------------------------------------
// Large sprites — loaded on demand by adventure screen
// ---------------------------------------------------------------------------
#define LG_IDLE_FRAMES 4
#define LG_WALK_FRAMES 4

static GBitmap *s_fox_lg_idle[LG_IDLE_FRAMES] = { NULL };
static GBitmap *s_fox_lg_walk[LG_WALK_FRAMES] = { NULL };

void sprites_load_large(void) {
  s_fox_lg_idle[0] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_IDLE_0);
  s_fox_lg_idle[1] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_IDLE_1);
  s_fox_lg_idle[2] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_IDLE_2);
  s_fox_lg_idle[3] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_IDLE_3);

  s_fox_lg_walk[0] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_WALK_0);
  s_fox_lg_walk[1] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_WALK_1);
  s_fox_lg_walk[2] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_WALK_2);
  s_fox_lg_walk[3] = gbitmap_create_with_resource(RESOURCE_ID_FOX_LG_WALK_3);
}

void sprites_unload_large(void) {
  for (int i = 0; i < LG_IDLE_FRAMES; i++) { if (s_fox_lg_idle[i]) { gbitmap_destroy(s_fox_lg_idle[i]); s_fox_lg_idle[i] = NULL; } }
  for (int i = 0; i < LG_WALK_FRAMES; i++) { if (s_fox_lg_walk[i]) { gbitmap_destroy(s_fox_lg_walk[i]); s_fox_lg_walk[i] = NULL; } }
}

GBitmap *sprites_get_fox_large(FoxState state, uint8_t frame) {
  switch (state) {
    case FOX_IDLE:  return s_fox_lg_idle[frame % LG_IDLE_FRAMES];
    case FOX_WALK:  return s_fox_lg_walk[frame % LG_WALK_FRAMES];
    default:        return NULL;
  }
}
