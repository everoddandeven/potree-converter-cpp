#include "las_info.h"
#include <unordered_map>

using namespace potree;

static const std::unordered_map<int, attribute_type> mapping = {
  {0, attribute_type::UNDEFINED},
  {1, attribute_type::UINT8},
  {2, attribute_type::INT8},
  {3, attribute_type::UINT16},
  {4, attribute_type::INT16},
  {5, attribute_type::UINT32},
  {6, attribute_type::INT32},
  {7, attribute_type::UINT64},
  {8, attribute_type::INT64},
  {9, attribute_type::FLOAT},
  {10, attribute_type::DOUBLE},

  {11, attribute_type::UINT8},
  {12, attribute_type::INT8},
  {13, attribute_type::UINT16},
  {14, attribute_type::INT16},
  {15, attribute_type::UINT32},
  {16, attribute_type::INT32},
  {17, attribute_type::UINT64},
  {18, attribute_type::INT64},
  {19, attribute_type::FLOAT},
  {20, attribute_type::DOUBLE},

  {21, attribute_type::UINT8},
  {22, attribute_type::INT8},
  {23, attribute_type::UINT16},
  {24, attribute_type::INT16},
  {25, attribute_type::UINT32},
  {26, attribute_type::INT32},
  {27, attribute_type::UINT64},
  {28, attribute_type::INT64},
  {29, attribute_type::FLOAT},
  {30, attribute_type::DOUBLE},
};


las_info las_info::parse(int type_id) {
  auto type_it = mapping.find(type_id);
	if (type_it == mapping.end()) 
    throw std::runtime_error("Unkown extra attribute type: " + std::to_string(type_id));
  
	attribute_type type = type_it->second;
  int numElements = 0;
  if (type_id <= 10) numElements = 1;
  else if (type_id <= 20) numElements = 2;
  else if (type_id <= 30) numElements = 3;
  
  las_info info;
  info.type = type;
  info.num_elements = numElements;

  return info;
}
