#pragma once

#include "attribute_type.h"
#include "utils/gen_utils.h"
#include "vector3.h"
#include <vector>

namespace potree {
  struct attribute {
    std::string name = "";
    std::string description = "";
    int size = 0;
    int numElements = 0;
    int elementSize = 0;
    attribute_type type = attribute_type::UNDEFINED;

    // TODO: should be type-dependent, not always double. won't work properly with 64 bit integers
    vector3 min = {gen_utils::INF, gen_utils::INF, gen_utils::INF};
    vector3 max = {-gen_utils::INF, -gen_utils::INF, -gen_utils::INF};

    vector3 scale = {1.0, 1.0, 1.0};
    vector3 offset = {0.0, 0.0, 0.0};

    // histogram that counts occurances of points with same attribute value.
    // only for 1 byte types, due to storage size
    std::vector<int64_t> histogram = std::vector<int64_t>(256, 0);

    attribute() { }

    attribute(std::string name, int size, int numElements, int elementSize, attribute_type type) {
      this->name = name;
      this->size = size;
      this->numElements = numElements;
      this->elementSize = elementSize;
      this->type = type;
    }
  };
}