#pragma once

#include "node.h"

namespace potree {
  struct sampler_state {
    int bytesPerPoint;
    double baseSpacing;
    vector3 scale;
    vector3 offset;

    std::function<void(node*)> writeAndUnload;
  };
}
