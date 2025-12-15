#pragma once

#include "bounding_box.h"
#include "attributes.h"

namespace potree {
  struct chunk : public bounding_box {
    std::string m_id;
    std::string m_file;
  };

  struct chunks : public bounding_box {
    std::vector<std::shared_ptr<chunk>> m_list;
    attributes m_attributes;

    chunks(const std::vector<std::shared_ptr<chunk>>& chunks, const vector3& min, const vector3& max) {
      m_list = chunks;
      this->min = min;
      this->max = max;
    }
  };

}
