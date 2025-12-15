#include "vector3.h"

using namespace potree;

vector3 vector3::parse(const json& bounding_box_json, const std::string& field) {
  vector3 v = {
    bounding_box_json[field][0].get<double>(),
    bounding_box_json[field][1].get<double>(),
    bounding_box_json[field][2].get<double>()
  };

  return v;
}

vector3 vector3::parse_max(const json& bounding_box_json) {
  return parse(bounding_box_json, "max");
}

vector3 vector3::parse_min(const json& bounding_box_json) {
  return parse(bounding_box_json, "min");
}
