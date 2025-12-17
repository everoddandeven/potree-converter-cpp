#include <random>
#include <chrono>
#include "sampler_random.h"
#include "sampler_point.h"
#include "sampler_cell_index.h"

using namespace potree;

void sampler_random::sample(node* n, attributes attrs, double base_spacing, std::function<void(node*)> on_complete, std::function<void(node*)> on_discard) {
  int bytesPerPoint = attrs.bytes;
  vector3& scale = attrs.posScale;
  vector3& offset = attrs.posOffset;

  n->traversePost([bytesPerPoint, base_spacing, scale, offset, &on_complete, &on_discard, attrs](const std::shared_ptr<potree::node>& node) {
    node->sampled = true;

    int64_t numPoints = node->numPoints;

    int64_t gridSize = 128;
    thread_local std::vector<int64_t> grid(gridSize* gridSize* gridSize, -1);
    thread_local int64_t iteration = 0;
    iteration++;

    auto max = node->max;
    auto min = node->min;
    auto size = max - min;
    auto scale = attrs.posScale;
    auto offset = attrs.posOffset;

    if (node->isLeaf()) {
      // a not particularly efficient approach to shuffling
      std::vector<int> indices(node->numPoints);
      for (int i = 0; i < node->numPoints; i++) {
        indices[i] = i;
      }

      unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
      shuffle(indices.begin(), indices.end(), std::default_random_engine(seed));
      auto buffer = std::make_shared<potree::buffer>(node->points->size);

      for (int i = 0; i < node->numPoints; i++) {
        int64_t sourceOffset = i * attrs.bytes;
        int64_t targetOffset = indices[i] * attrs.bytes;

        memcpy(buffer->data_u8 + targetOffset, node->points->data_u8 + sourceOffset, attrs.bytes);
      }

      node->points = buffer;
      
      return false;
    }

    // SAMPLING
    // first, check for each point whether it's accepted or rejected
    // save result in an array with one element for each point

    std::vector<std::vector<int8_t>> acceptedChildPointFlags;
    std::vector<int64_t> numRejectedPerChild;
    int64_t numAccepted = 0;
    for (int childIndex = 0; childIndex < 8; childIndex++) {
      auto child = node->children[childIndex];

      if (child == nullptr) {
        acceptedChildPointFlags.push_back({});
        numRejectedPerChild.push_back({});

        continue;
      }

      std::vector<int8_t> acceptedFlags(child->numPoints, 0);
      int64_t numRejected = 0;

      for (int i = 0; i < child->numPoints; i++) {
        int64_t pointOffset = i * attrs.bytes;
        int32_t* xyz = reinterpret_cast<int32_t*>(child->points->data_u8 + pointOffset);

        double x = (xyz[0] * scale.x) + offset.x;
        double y = (xyz[1] * scale.y) + offset.y;
        double z = (xyz[2] * scale.z) + offset.z;

        auto cellIndex = sampler_cell_index::convert(min, size, { x, y, z }, gridSize);
        auto& gridValue = grid[cellIndex.index];
        static double all = sqrt(3.0);
        bool isAccepted = false;

        if (child->numPoints < 100 || (cellIndex.distance < 0.7 * all && gridValue < iteration)) {
          isAccepted = true;
        }

        if (isAccepted) {
          gridValue = iteration;
          numAccepted++;
        } 
        else {
          numRejected++;
        }

        acceptedFlags[i] = isAccepted ? 1 : 0;
      }

      acceptedChildPointFlags.push_back(acceptedFlags);
      numRejectedPerChild.push_back(numRejected);
    }

    auto accepted = std::make_shared<potree::buffer>(numAccepted * attrs.bytes);
    for (int childIndex = 0; childIndex < 8; childIndex++) {
      auto child = node->children[childIndex];

      if (child == nullptr) continue;

      auto numRejected = numRejectedPerChild[childIndex];
      auto& acceptedFlags = acceptedChildPointFlags[childIndex];
      auto rejected = std::make_shared<potree::buffer>(numRejected * attrs.bytes);

      for (int i = 0; i < child->numPoints; i++) {
        auto isAccepted = acceptedFlags[i];
        int64_t pointOffset = i * attrs.bytes;

        if (isAccepted) {
          accepted->write(child->points->data_u8 + pointOffset, attrs.bytes);
        } else {
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
