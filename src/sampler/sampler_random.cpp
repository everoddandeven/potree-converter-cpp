#include "sampler_random.h"
#include "sampler_point.h"

using namespace potree;

void traverse(node* n, std::function<void(node*)> callback) {
  for(auto& child : n->children) {
    if (child != nullptr && child->sampled) traverse(child.get(), callback);
  }
  callback(n);
}

void sampler_random::sample(node* n, attributes attrs, double base_spacing, std::function<void(node*)> on_complete, std::function<void(node*)> on_discard) {

}
