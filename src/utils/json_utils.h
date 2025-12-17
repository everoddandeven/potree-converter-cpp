#pragma once

#include <sstream>
#include <vector>
#include "gen_utils.h"

namespace potree {
namespace json_utils {

  static inline std::string to_json(const std::vector<double>& values) {
  	std::stringstream ss;
		ss << "[";

		for (int i = 0; i < values.size(); i++) {

			ss << gen_utils::to_digits(values[i]);

			if (i < values.size() - 1) {
				ss << ", ";
			}
		}
		ss << "]";

		return ss.str();  
  }

  static inline std::string to_json(const std::vector<int64_t>& values) {
  	std::stringstream ss;
		ss << "[";

		for (int i = 0; i < values.size(); i++) {

			ss << values[i];

			if (i < values.size() - 1) {
				ss << ", ";
			}
		}
		ss << "]";

		return ss.str();  
  }

  static inline std::string str_value(const std::string& value) {
    return "\"" + value + "\"";
  }

  static inline std::string tab(int tabs) {
    return std::string(tabs, '\t');
  }
}
}