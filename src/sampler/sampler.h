#pragma once

#include "geometry/attributes.h"
#include "geometry/node.h"

namespace potree {
  struct sampler {
    sampler() { }

    virtual void sample(node* n, attributes attrs, double base_spacing, std::function<void(node*)> on_complete, std::function<void(node*)> on_discard) = 0;
  };
}