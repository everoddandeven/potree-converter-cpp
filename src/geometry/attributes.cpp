#include "attributes.h"
#include "utils/gen_utils.h"
#include "utils/string_utils.h"
#include "utils/json_utils.h"
#include "utils/attribute_utils.h"

using namespace potree;

attributes::attributes(std::vector<attribute> attributes) {
  m_list = attributes;

  for (auto& attribute : attributes) {
    bytes += attribute.size;
  }
}

int attributes::get_offset(std::string name) const {
  int offset = 0;

  for (auto& attribute : m_list) {

    if (attribute.name == name) {
      return offset;
    }

    offset += attribute.size;
  }

  return -1;
}

attribute* attributes::get(std::string name) {
  for (auto& attribute : m_list) {
    if (attribute.name == name) {
      return &attribute;
    }
  }
  
  return nullptr;
}

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
  for (auto attribute : m_list) {
    ss << string_utils::right_pad(attribute.name, c0)
      << string_utils::left_pad(gen_utils::format_number(offset), c1)
      << string_utils::left_pad(gen_utils::format_number(attribute.size), c2)
      << std::endl;

    offset += attribute.size;
  }
  ss << std::string(ct, '=') <<  std::endl;

  //cout << "bytes per point: " << attributes.bytes << std::endl;
  ss << string_utils::left_pad(gen_utils::format_number(bytes), ct) << std::endl;
  ss << std::string(ct, '=') << std::endl;

  return ss.str();
}

std::string attributes::to_json() const {
  std::stringstream ss;
  ss << "[" << std::endl;

  for (int i = 0; i < m_list.size(); i++) {
    auto& attribute = m_list[i];

    if (i == 0) {
      ss << json_utils::tab(2) << "{" << std::endl;
    }

    ss << json_utils::tab(3) << json_utils::str_value("name") << ": " << json_utils::str_value(attribute.name) << "," << std::endl;
    ss << json_utils::tab(3) << json_utils::str_value("description") << ": " << json_utils::str_value(attribute.description) << "," << std::endl;
    ss << json_utils::tab(3) << json_utils::str_value("size") << ": " << attribute.size << "," << std::endl;
    ss << json_utils::tab(3) << json_utils::str_value("numElements") << ": " << attribute.numElements << "," << std::endl;
    ss << json_utils::tab(3) << json_utils::str_value("elementSize") << ": " << attribute.elementSize << "," << std::endl;
    ss << json_utils::tab(3) << json_utils::str_value("type") << ": " << json_utils::str_value(attribute_utils::get_name(attribute.type)) << "," << std::endl;

    bool empty_histogram = true;
    for(int i = 0; i < attribute.histogram.size(); i++){
      if(attribute.histogram[i] != 0){
        empty_histogram = false;
      }
    }

    if(attribute.size == 1 && !empty_histogram){
      ss << json_utils::tab(3) << json_utils::str_value("histogram") << ": " << json_utils::to_json(attribute.histogram) << ", " << std::endl;
    }

    if (attribute.numElements == 1) {
      ss << json_utils::tab(3) << json_utils::str_value("min") << ": " << json_utils::to_json(std::vector<double>{ attribute.min.x }) << "," << std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("max") << ": " << json_utils::to_json(std::vector<double>{ attribute.max.x }) << ","<< std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("scale") << ": " << json_utils::to_json(std::vector<double>{ attribute.scale.x }) << ","<< std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("offset") << ": " << json_utils::to_json(std::vector<double>{ attribute.offset.x }) << std::endl;
    } 
    else if (attribute.numElements == 2) {
      ss << json_utils::tab(3) << json_utils::str_value("min") << ": " << json_utils::to_json(std::vector<double>{ attribute.min.x, attribute.min.y }) << "," << std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("max") << ": " << json_utils::to_json(std::vector<double>{ attribute.max.x, attribute.max.y }) << ","<< std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("scale") << ": " << json_utils::to_json(std::vector<double>{ attribute.scale.x, attribute.scale.y }) << ","<< std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("offset") << ": " << json_utils::to_json(std::vector<double>{ attribute.offset.x, attribute.offset.y }) << std::endl;
    } 
    else if (attribute.numElements == 3) {
      ss << json_utils::tab(3) << json_utils::str_value("min") << ": " << json_utils::to_json(std::vector<double>{ attribute.min.x, attribute.min.y, attribute.min.z }) << "," << std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("max") << ": " << json_utils::to_json(std::vector<double>{ attribute.max.x, attribute.max.y, attribute.max.z }) << ","<< std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("scale") << ": " << json_utils::to_json(std::vector<double>{ attribute.scale.x, attribute.scale.y, attribute.scale.z }) << ","<< std::endl;
      ss << json_utils::tab(3) << json_utils::str_value("offset") << ": " << json_utils::to_json(std::vector<double>{ attribute.offset.x, attribute.offset.y, attribute.offset.z }) << std::endl;
    }

    if (i < m_list.size() - 1) {
      ss << json_utils::tab(2) << "},{" << std::endl;
    } 
    else {
      ss << json_utils::tab(2) << "}" << std::endl;
    }

  }
  
  ss << json_utils::tab(1) << "]";

  return ss.str();
}

int64_t attributes::get_index(uint8_t* xyz_data, const vector3& scale, int64_t grid_size, const vector3& size, const vector3& min, int64_t point_offset) const {
  int32_t* xyz = reinterpret_cast<int32_t*>(&xyz_data[0] + point_offset);
  int32_t X = xyz[0];
  int32_t Y = xyz[1];
  int32_t Z = xyz[2];

  double ux = (double(X) * scale.x + m_pos_offset.x - min.x) / size.x;
  double uy = (double(Y) * scale.y + m_pos_offset.y - min.y) / size.y;
  double uz = (double(Z) * scale.z + m_pos_offset.z - min.z) / size.z;
  double d_grid_size = double(grid_size);

  int64_t ix = int64_t(std::min(d_grid_size * ux, d_grid_size - 1.0));
  int64_t iy = int64_t(std::min(d_grid_size * uy, d_grid_size - 1.0));
  int64_t iz = int64_t(std::min(d_grid_size * uz, d_grid_size - 1.0));

  int64_t index = ix + iy * grid_size + iz * grid_size * grid_size;

  return index;
}

