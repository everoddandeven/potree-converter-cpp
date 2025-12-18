#pragma once

#include "node.h"

namespace potree {
  struct sampler_state {
    int m_bytes_per_point;
    double m_base_spacing;
    vector3 m_scale;
    vector3 m_offset;

    std::function<void(node*)> writeAndUnload;
  };
}
