#pragma once

#include "attribute.h"
#include "file_source.h"

namespace potree {
  
  struct attributes {

    std::vector<attribute> list;
    int bytes = 0;

    vector3 posScale = vector3{ 1.0, 1.0, 1.0 };
    vector3 posOffset = vector3{ 0.0, 0.0, 0.0 };

    attributes() { }

    attributes(std::vector<attribute> attributes) {
      this->list = attributes;

      for (auto& attribute : attributes) {
        bytes += attribute.size;
      }
    }

    int getOffset(std::string name) {
      int offset = 0;

      for (auto& attribute : list) {

        if (attribute.name == name) {
          return offset;
        }

        offset += attribute.size;
      }

      return -1;
    }

    attribute* get(std::string name) {
      for (auto& attribute : list) {
        if (attribute.name == name) {
          return &attribute;
        }
      }
      
      return nullptr;
    }

    std::string to_string() const;

  };

}