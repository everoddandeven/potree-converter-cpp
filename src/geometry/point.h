#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace potree {

  struct point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    static double square_distance(const point& a, const point& b);
  };

  struct int64_t_point {
    int64_t x = 0;
    int64_t y = 0;
    int64_t z = 0;

    static std::pair<int64_t_point, int64_t_point> compute_box(const std::vector<int64_t_point>& pts);
  };

  struct int32_t_point {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
  };

  struct colored_point : point {
    uint16_t r = 0;
		uint16_t g = 0;
		uint16_t b = 0;
  };

  struct las_point_f2 : public int32_t_point {
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

  struct sample_point : public point {
    int32_t pointIndex;
    int32_t childIndex;
  };

}