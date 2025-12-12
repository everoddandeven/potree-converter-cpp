#pragma once
#include <cstdint>

namespace potree {
  struct las_point_f2 {
    int32_t x;
    int32_t y;
    int32_t z;
    uint16_t intensity;
    uint8_t returnNumber;
    uint8_t classification;
    uint8_t scanAngleRank;
    uint8_t userData;
    uint16_t pointSourceID;
    uint16_t r;
    uint16_t g;
    uint16_t b;
  };
}
