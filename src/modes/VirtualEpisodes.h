#pragma once

#include <stdint.h>

struct VirtualEpisode {
  uint16_t episode;
  uint8_t track;
};

// Physical folder 99 contains one complete episode per file, with deliberate gaps.
static constexpr VirtualEpisode virtualEpisodes[] = {
  {99, 1},
  {100, 2},
  {101, 3},
  {102, 4},
  {103, 5},
  {104, 6},
  {105, 7},
  {106, 8},
  {108, 9},
  {109, 10},
  {110, 11},
  {111, 12},
  {119, 13},
  {121, 14},
  {122, 15},
  {123, 16},
  {124, 17},
  {125, 18},
  {126, 19},
  {127, 20},
  {128, 21},
  {130, 22},
  {132, 23},
  {133, 24},
  {134, 25},
  {135, 26},
  {137, 27},
  {138, 28},
  {139, 29},
  {140, 30},
  {142, 31},
  {143, 32},
  {148, 33},
  {153, 34},
  {154, 35},
  {155, 36},
  {160, 37},
  {161, 38},
  {162, 39},
  {165, 40},
  {170, 41},
  {171, 42},
  {175, 43},
  {179, 44},
  {180, 45},
  {999, 46},
};

inline bool mapVirtualEpisodeToTrack(uint16_t episode, uint8_t& track) {
  for (const auto& entry : virtualEpisodes) {
    if (entry.episode == episode) {
      track = entry.track;
      return true;
    }
  }
  return false;
}
