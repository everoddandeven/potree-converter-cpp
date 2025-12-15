#pragma once

#include "vector3.h"
#include "utils/gen_utils.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace potree {

  struct bounding_box {
    vector3 min;
    vector3 max;

    bounding_box() {
      this->min = { gen_utils::INF, gen_utils::INF, gen_utils::INF };
      this->max = { -gen_utils::INF, -gen_utils::INF, -gen_utils::INF };
    }

    bounding_box(vector3 min, vector3 max) {
      this->min = min;
      this->max = max;
    }

    static bounding_box child_of(vector3 min, vector3 max, int index);
    static bounding_box parse(const json& metadata);
  };

}

