#pragma once

#include "common/file_source.h"
#include "attribute.h"

namespace potree {
  
  struct attributes {

    std::vector<attribute> m_list;
    int bytes = 0;
    vector3 m_pos_scale = { 1.0, 1.0, 1.0 };
    vector3 m_pos_offset = { 0.0, 0.0, 0.0 };

    attributes() { }
    attributes(std::vector<attribute> attributes);

    attribute* get(std::string name);
    int get_offset(std::string name) const;

    std::string to_string() const;
    std::string to_json() const;

  };

}