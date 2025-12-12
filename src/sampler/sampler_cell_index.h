#pragma once

#include <cstdint>
#include "common/vector3.h"


namespace potree {

  struct sampler_cell_index {
    int64_t index = -1;
    double distance = 0.0;

    static sampler_cell_index convert(vector3 point, vector3 min, vector3 size, int64_t gridSize);
  };

}