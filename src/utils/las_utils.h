#pragma once

#include "common/attributes.h"
#include "las/las_header.h"

namespace potree {
namespace las_utils {

  std::vector<attribute> parse_extra_attributes(const las_header& header);
  std::vector<attribute> compute_output_attributes(const las_header& header);
  attributes compute_output_attributes(std::vector<file_source>& sources, std::vector<std::string>& requested_attributes);

}
}