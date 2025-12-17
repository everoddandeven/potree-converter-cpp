#pragma once

#include <cstdint>
#include "vector3.h"


namespace potree {

  struct cell_index {
    int64_t index = -1;
    double distance = 0.0;

    static cell_index convert(vector3 point, vector3 min, vector3 size, int64_t grid_size);
  };

}