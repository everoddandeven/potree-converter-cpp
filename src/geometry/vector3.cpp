#include "vector3.h"

using namespace potree;

vector3::vector3(double x, double y, double z) {
  this->x = x;
  this->y = y;
  this->z = z;
}

vector3::vector3(double value[3]) {
  this->x = value[0];
  this->y = value[1];
  this->z = value[2];
}

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

std::string vector3::to_string() const {
  auto digits = std::numeric_limits<double>::max_digits10;
  std::stringstream ss;
  ss << std::setprecision(digits);
  ss << x << ", " << y << ", " << z;
  return ss.str();
}

std::string vector3::to_json() const {
  return "[" + gen_utils::to_digits(x) + ", " + gen_utils::to_digits(y) + ", " + gen_utils::to_digits(z) + "]";
}
