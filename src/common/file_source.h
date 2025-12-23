#pragma once 

#include "geometry/vector3.h"

namespace potree {
  struct file_source {
    std::string path;
    uint64_t filesize;
    uint64_t numPoints = 0;
    int bytesPerPoint = 0;
    vector3 min;
    vector3 max;
  };

  struct file_source_container {
    std::string m_name;
    std::vector<file_source> m_files;
  };
}
