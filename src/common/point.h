#pragma once
#include <cstdint>

namespace potree {

  struct point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
  };

  struct colored_point : point {
    uint16_t r = 0;
		uint16_t g = 0;
		uint16_t b = 0;
  };

}