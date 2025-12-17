#pragma once

#include "geometry/point.h"
#include "sampler.h"

namespace potree {
  struct sampler_poisson : public sampler {
  public:
    void sample(node* n, attributes attrs, double base_spacing, std::function<void(node*)> on_complete, std::function<void(node*)> on_discard) override;
  private:
    bool accept(const point& candidate, const vector3& center, double spacing, int64_t num_accepted, std::vector<sample_point>& accepted);
  };
}
