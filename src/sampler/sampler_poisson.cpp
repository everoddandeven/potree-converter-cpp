#include "sampler_poisson.h"

using namespace potree;

bool sampler_poisson::accept(
  const point& candidate, const vector3& center, double spacing,
  int64_t num_accepted, std::vector<sampler_point>& accepted
) {
  auto cx = candidate.x - center.x;
  auto cy = candidate.y - center.y;
  auto cz = candidate.z - center.z;
  auto cdd = cx * cx + cy * cy + cz * cz;
  auto cd = sqrt(cdd);
  auto limit = (cd - spacing);
  auto limitSquared = limit * limit;

  int64_t j = 0;
  for(int64_t i = num_accepted -1; i >= 0; i--) {
    auto& p = accepted[i];
    // check distance to center
    auto px = p.x - center.x;
    auto py = p.y - center.y;
    auto pz = p.z - center.z;
    auto pdd = px * px + py * py + pz * pz;

    // stop when differences to center between candidate and accepted exceeds the spacing
    // any other previously accepted point will be even closer to the center.
    if (pdd < limitSquared) {
      return true;
    }

    double dd = point::square_distance(p, candidate);
    if (dd < spacing * spacing) return false;

    j++;
    // also put a limit at x distance checks
    if (j > 10'000) return true;
  }

  return true;
}

void sampler_poisson::sample(node* n, attributes attrs, double base_spacing, std::function<void(node*)> on_complete, std::function<void(node*)> on_discard) {
  int bytesPerPoint = attrs.bytes;
  vector3& scale = attrs.posScale;
  vector3& offset = attrs.posOffset;

  n->traversePost([this, bytesPerPoint, base_spacing, scale, offset, &on_complete, &on_discard, attrs](potree::node* node) {
    node->sampled = true;

    int64_t numPoints = node->numPoints;

    auto max = node->max;
    auto min = node->min;
    auto size = max - min;
    auto scale = attrs.posScale;
    auto offset = attrs.posOffset;

    if (node->isLeaf()) {      
      return false;
    }

    // SAMPLING
    // first, check for each point whether it's accepted or rejected
    // save result in an array with one element for each point

    int64_t numPointsInChildren = 0; // specific
    for (auto child : node->children) {
      if (child == nullptr) {
        continue;
      }

      numPointsInChildren += child->numPoints;
    } // specific
    
    std::vector<point> points; // specific
    points.reserve(numPointsInChildren); // specific

    std::vector<std::vector<int8_t>> acceptedChildPointFlags;
    std::vector<int64_t> numRejectedPerChild(8, 0); // specific
    int64_t numAccepted = 0;
    for (int childIndex = 0; childIndex < 8; childIndex++) {
      auto child = node->children[childIndex];

      if (child == nullptr) {
        acceptedChildPointFlags.push_back({});
        numRejectedPerChild.push_back({});

        continue;
      }

      std::vector<int8_t> acceptedFlags(child->numPoints, 0);

      for (int i = 0; i < child->numPoints; i++) {
        int64_t pointOffset = i * attrs.bytes;
        int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

        double x = (xyz[0] * scale.x) + offset.x;
        double y = (xyz[1] * scale.y) + offset.y;
        double z = (xyz[2] * scale.z) + offset.z;

        sampler_point p = { x, y, z, i, childIndex };
        points.push_back(p);
      }

      acceptedChildPointFlags.push_back(acceptedFlags);
    }

    double spacing = base_spacing / pow(2.0, node->get_level());
    node->sort_by_distance_to_center(points);
    const auto center = node->get_center();
    thread_local std::vector<sampler_point> accepted_v(1'000'000);
    int64_t num_accepted = 0;

    for(const point& p : points) {
      auto point = static_cast<sampler_point>(p);

      if (accept(point, center, spacing, num_accepted, accepted_v)) {
        accepted_v[num_accepted] = point;
        num_accepted++;
        acceptedChildPointFlags[point.childIndex][point.pointIndex] = 1;
      }
      else {
        numRejectedPerChild[point.childIndex]++;
        acceptedChildPointFlags[point.childIndex][point.pointIndex] = 0;
      }
    }

    auto accepted = std::make_shared<potree::buffer>(num_accepted * attrs.bytes);

    for (int64_t childIndex = 0; childIndex < 8; childIndex++) {
      auto child = node->children[childIndex];

      if (child == nullptr) continue;

      int64_t numRejected = numRejectedPerChild[childIndex];
      auto& acceptedFlags = acceptedChildPointFlags[childIndex];
      auto rejected = std::make_shared<potree::buffer>(numRejected * attrs.bytes);

      for (int64_t i = 0; i < child->numPoints; i++) {
        int8_t isAccepted = acceptedFlags[i];
        int64_t pointOffset = i * attrs.bytes;

        if (isAccepted) {
          accepted->write(child->points->data_u8 + pointOffset, attrs.bytes);
          // rejected->write(child->points->data_u8 + pointOffset, attributes.bytes);
        } 
        else {
          rejected->write(child->points->data_u8 + pointOffset, attrs.bytes);
        }
      }

      if (numRejected == 0 && child->isLeaf()) {
        on_discard(child.get());
        node->children[childIndex] = nullptr;
      } 
      if (numRejected > 0) {
        child->points = rejected;
        child->numPoints = numRejected;

        on_complete(child.get());
      } 
      else if (numRejected == 0) {
        // the parent has taken all points from this child, 
        // so make this child an empty inner node.
        // Otherwise, the hierarchy file will claim that 
        // this node has points but because it doesn't have any,
        // decompressing the nonexistent point buffer fails
        // https://github.com/potree/potree/issues/1125
        child->points = nullptr;
        child->numPoints = 0;
        on_complete(child.get());
      }
    }

    node->points = accepted;
    node->numPoints = numAccepted;

    return true;
  });
}
