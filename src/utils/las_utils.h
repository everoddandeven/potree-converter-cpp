#pragma once

#include <functional>
#include "laszip/laszip_api.h"
#include "common/attributes.h"
#include "las/las_header.h"

namespace potree {
namespace las_utils {
  typedef std::vector<colored_point> point_level;
  typedef std::vector<point_level> point_levels;
  typedef std::function<void(int64_t)> attribute_handler;
  std::vector<attribute> parse_extra_attributes(const las_header& header);
  std::vector<attribute> compute_output_attributes(const las_header& header);
  attributes compute_output_attributes(std::vector<file_source>& sources, std::vector<std::string>& requested_attributes);
  void save(const laszip_header* header, const std::vector<colored_point>& points, const std::string& target);
  void save(const std::string& target, const point_level& points, const vector3& min, const vector3& max);
  void to_laz(const std::string& potree_path);
  std::vector<attribute_handler> create_attribute_handlers(
    laszip_header* header, uint8_t* data, laszip_point* point, 
    attributes& inputAttributes, attributes& outputAttributes
  );
}
}