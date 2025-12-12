#pragma once

#include "common/vector3.h"
#include "las_vlr.h"

namespace potree {
  struct las_header {
    vector3 min;
    vector3 max;
    vector3 scale;
    vector3 offset;

    int64_t numPoints = 0;

    int pointDataFormat = -1;

    std::vector<las_vlr> vlrs;

    static las_header load(const std::string& path);
  };
}