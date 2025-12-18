#pragma once

#include "sampler.h"

namespace potree {
  struct sampler_random : public sampler {
    void sample(const std::shared_ptr<potree::node>& n, attributes& attrs, double base_spacing, node_function on_complete, node_function on_discard) override;
  };
}