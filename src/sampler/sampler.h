#pragma once

#include "geometry/attributes.h"
#include "geometry/node.h"

namespace potree {

  typedef std::function<void(const std::shared_ptr<potree::node>&)> node_function;

  struct sampler {
    sampler() { }

    virtual void sample(const std::shared_ptr<potree::node>& n, attributes& attrs, double base_spacing, node_function on_complete, node_function on_discard) = 0;
  };
}