#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// --- Constants ---
#define MAX_SEGMENTS  8
#define NUM_BIOMES    6
#define NUM_STATS     6
#define MAX_STAT      999

// --- Persist Keys ---
#define PERSIST_KEY_PET           1
#define PERSIST_KEY_ADVENTURE     2
#define PERSIST_KEY_WORKER_STEPS  3
#define PERSIST_KEY_PENDING_ENCOUNTER 4

#define NUM_ENCOUNTERS 5

// --- Stat index constants ---
#define STAT_IDX_STR  0
#define STAT_IDX_DEX  1
#define STAT_IDX_AGI  2
#define STAT_IDX_VIT  3
#define STAT_IDX_INT  4
#define STAT_IDX_LUK  5

// --- Biome Types ---
typedef enum {
  BIOME_PLAINS   = 0,
  BIOME_FOREST   = 1,
  BIOME_WATER    = 2,
  BIOME_MOUNTAIN = 3,
  BIOME_CAVE     = 4,
  BIOME_STORM    = 5,
} BiomeType;

// --- Pet ---
typedef struct {
  char     name[12];
  uint16_t level;
  uint32_t xp;
  uint32_t xp_next_level;   // precomputed: floor(150 * level^1.4)
  uint16_t upgrade_points;  // unspent points (uint16 — late game earns 22+/level)
  uint16_t str;
  uint16_t dex;
  uint16_t agi;
  uint16_t vit;
  uint16_t intel;           // 'int' is a C reserved word
  uint16_t luk;
} Pet;

// --- Adventure ---
typedef struct {
  bool     active;
  uint8_t  num_segments;
  uint8_t  current_segment;
  uint8_t  segments[MAX_SEGMENTS];          // BiomeType values
  uint32_t segment_length[MAX_SEGMENTS];    // effective steps needed (pre-computed at init)
  uint32_t segment_progress[MAX_SEGMENTS];  // raw steps accumulated
  uint32_t total_xp_earned;
  uint8_t  encounters_total;
} Adventure;
