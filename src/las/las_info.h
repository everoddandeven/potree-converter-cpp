#pragma once

#include "geometry/vector3.h"
#include "geometry/attributes.h"

namespace potree {

  struct las_info {
    attribute_type type = attribute_type::UNDEFINED;
    int num_elements = 0;

    static las_info parse(int type_id);
  };

}