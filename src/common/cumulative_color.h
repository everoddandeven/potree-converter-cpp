#pragma once
#include <cstdint>

namespace potree {
  
  struct cumulative_color {
    int64_t r = 0;
    int64_t g = 0;
    int64_t b = 0;
    int64_t w = 0;
  };
}