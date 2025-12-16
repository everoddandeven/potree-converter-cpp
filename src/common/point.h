#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace potree {

  struct point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
  };

  struct int64_t_point {
    int64_t x = 0;
    int64_t y = 0;
    int64_t z = 0;

    static std::pair<int64_t_point, int64_t_point> compute_box(const std::vector<int64_t_point>& pts);
  };

  struct colored_point : point {
    uint16_t r = 0;
		uint16_t g = 0;
		uint16_t b = 0;
  };

}