#pragma once

#include "common/vector3.h"
#include "common/attributes.h"

namespace potree {

  struct las_info {
    attribute_type type = attribute_type::UNDEFINED;
    int num_elements = 0;

    static las_info parse(int type_id);
  };

}