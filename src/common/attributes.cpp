#include "attributes.h"
#include "utils/gen_utils.h"
#include "utils/string_utils.h"


using namespace potree;

std::string attributes::to_string() const {
  std::stringstream ss;

  ss << std::endl << "output attributes: " << std::endl;

  int c0 = 30;
  int c1 = 10;
  int c2 = 8;
  int ct = c0 + c1 + c2;

  ss << string_utils::right_pad("name", c0) << string_utils::left_pad("offset", c1) << string_utils::left_pad("size", c2) << std::endl;
  ss << std::string(ct, '=') << std::endl;

  int offset = 0;
  for (auto attribute : list) {
    ss << string_utils::right_pad(attribute.name, c0)
      << string_utils::left_pad(gen_utils::format_number(offset), c1)
      << string_utils::left_pad(gen_utils::format_number(attribute.size), c2)
      << std::endl;

    offset += attribute.size;
  }
  ss << std::string(ct, '=') <<  std::endl;

  //cout << "bytes per point: " << attributes.bytes << endl;
  ss << string_utils::left_pad(gen_utils::format_number(bytes), ct) << std::endl;
  ss << std::string(ct, '=') << std::endl;

  return ss.str();
}
