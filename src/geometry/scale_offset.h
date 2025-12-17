#pragma once

#include "vector3.h"

namespace potree {
  struct scale_offset {
    vector3 scale;
    vector3 offset;

    static scale_offset compute(const vector3& min, const vector3& max, const vector3& target_scale);
  };
}
