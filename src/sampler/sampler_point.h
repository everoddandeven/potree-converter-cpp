#pragma once
#include <cstdint>
#include "common/point.h"

namespace potree {

  struct sampler_point : public point {
    int32_t pointIndex;
    int32_t childIndex;
  };

}
