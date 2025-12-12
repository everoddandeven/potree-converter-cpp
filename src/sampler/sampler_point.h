#pragma once
#include <cstdint>

namespace potree {

  struct sampler_point {
    double x;
    double y;
    double z;
    int32_t pointIndex;
    int32_t childIndex;
  };

}
