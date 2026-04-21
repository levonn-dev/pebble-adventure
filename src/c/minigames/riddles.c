/*
 * SPDX-FileCopyrightText: 2026 levonn-dev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "riddles.h"
#include <stdlib.h>

static const Riddle s_riddles[NUM_RIDDLES] = {
  { "What has hands but can't clap?",
    { "Clock", "Gloves", "Crab", "Tree" }, 0 },
  { "What has a head and tail but no body?",
    { "Snake", "Coin", "Arrow", "Nail" }, 1 },
  { "What gets wetter the more it dries?",
    { "Rain", "Towel", "Sponge", "Ice" }, 1 },
  { "What can you catch but not throw?",
    { "Ball", "Fish", "Cold", "Bus" }, 2 },
  { "What has keys but no locks?",
    { "Map", "Piano", "Jail", "Code" }, 1 },
  { "What goes up but never comes down?",
    { "Balloon", "Bird", "Age", "Rocket" }, 2 },
  { "What has teeth but cannot bite?",
    { "Shark", "Comb", "Saw", "Gear" }, 1 },
  { "What can travel the world in a corner?",
    { "Stamp", "Coin", "Map", "Photo" }, 0 },
  { "What has one eye but cannot see?",
    { "Pirate", "Needle", "Cyclops", "Potato" }, 1 },
  { "What is full of holes but holds water?",
    { "Net", "Bucket", "Sponge", "Pipe" }, 2 },
  { "What runs but never walks?",
    { "Dog", "Water", "Clock", "Wind" }, 1 },
  { "What can you break without touching?",
    { "Glass", "Egg", "Promise", "Ice" }, 2 },
  { "What has a neck but no head?",
    { "Giraffe", "Snake", "Bottle", "Screw" }, 2 },
  { "What gets shorter as it grows older?",
    { "Shadow", "Candle", "Day", "Hair" }, 1 },
  { "What has words but never speaks?",
    { "Radio", "Book", "Phone", "Parrot" }, 1 },
  { "What invention lets you look through a wall?",
    { "X-ray", "Drill", "Window", "Camera" }, 2 },
  { "What can fill a room but takes no space?",
    { "Air", "Light", "Fire", "Water" }, 1 },
  { "What can you break without hitting it?",
    { "Time", "Table", "Promise", "Wind" }, 2 },
  { "What is always in front of you but unseen?",
    { "Shadow", "Nose", "Future", "Air" }, 2 },
  { "What belongs to you but others use more?",
    { "Phone", "Name", "Car", "Money" }, 1 },
};

const Riddle *riddle_get(uint8_t index) {
  if (index >= NUM_RIDDLES) return &s_riddles[0];
  return &s_riddles[index];
}

uint8_t riddle_random_index(void) {
  return (uint8_t)(rand() % NUM_RIDDLES);
}
