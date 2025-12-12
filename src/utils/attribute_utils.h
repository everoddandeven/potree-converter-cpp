#pragma once

#include "common/attribute_type.h"

namespace potree {
namespace attribute_utils {

  int get_size(attribute_type type);
  std::string get_name(attribute_type type);
  attribute_type get_type(const std::string& name);

}
}
