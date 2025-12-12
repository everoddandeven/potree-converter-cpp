#include <unordered_map>
#include <stdexcept>
#include "attribute_utils.h"

using namespace potree;

static std::unordered_map<attribute_type, int> mapping = {
  {attribute_type::UNDEFINED, 0},
  {attribute_type::UINT8, 1},
  {attribute_type::UINT16, 2},
  {attribute_type::UINT32, 4},
  {attribute_type::UINT64, 8},
  {attribute_type::INT8, 1},
  {attribute_type::INT16, 2},
  {attribute_type::INT32, 4},
  {attribute_type::INT64, 8},
  {attribute_type::FLOAT, 4},
  {attribute_type::DOUBLE, 8},
};

int attribute_utils::get_size(attribute_type type) {
  return mapping[type];
}

std::string attribute_utils::get_name(attribute_type type) {
  if (type == attribute_type::INT8) return "int8";
	else if (type == attribute_type::INT16) return "int16";
	else if (type == attribute_type::INT32) return "int32";
	else if (type == attribute_type::INT64) return "int64";
	else if (type == attribute_type::UINT8) return "uint8";
	else if (type == attribute_type::UINT16) return "uint16";
	else if (type == attribute_type::UINT32) return "uint32";
	else if (type == attribute_type::UINT64) return "uint64";
	else if (type == attribute_type::FLOAT) return "float";
	else if (type == attribute_type::DOUBLE) return "double";
	else if (type == attribute_type::UNDEFINED) return "undefined";
	throw std::runtime_error("Invalid attribute_type provided");
}

attribute_type attribute_utils::get_type(const std::string& name) {
	if (name == "int8") return attribute_type::INT8;
	else if (name == "int16") return attribute_type::INT16;
	else if (name == "int32") return attribute_type::INT32;
	else if (name == "int64") return attribute_type::INT64;
	else if (name == "uint8") return attribute_type::UINT8;
	else if (name == "uint16") return attribute_type::UINT16;
	else if (name == "uint32") return attribute_type::UINT32;
	else if (name == "uint64") return attribute_type::UINT64;
	else if (name == "float") return attribute_type::FLOAT;
	else if (name == "double") return attribute_type::DOUBLE;
	else if (name == "undefined") return attribute_type::UNDEFINED;
  throw std::runtime_error("Unkown attribute_type:" + name);
}
