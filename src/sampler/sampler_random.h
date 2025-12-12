#pragma once

#include "sampler.h"

namespace potree {
  struct sampler_random : public sampler {
    void sample(node* n, attributes attrs, double base_spacing, std::function<void(node*)> on_complete, std::function<void(node*)> on_discard) override;
  };
}