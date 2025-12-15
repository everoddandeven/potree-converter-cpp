#pragma once

#include "vector3.h"
#include "utils/gen_utils.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace potree {

  struct bounding_box {
    vector3 min;
    vector3 max;

    bounding_box();
    bounding_box(const vector3& min, const vector3& max);
    bounding_box child_of(int index) const;

    static bounding_box child_of(const vector3& min, const vector3& max, int index);
    static bounding_box parse(const json& metadata);
  };

}

