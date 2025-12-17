#pragma once

#include <string>
#include <cmath>
#include <limits>
#include <sstream>
#include <algorithm>
#include <string>
#include <iomanip>
#include "utils/gen_utils.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;

namespace potree {

	struct vector3 {
	public:
		double x = double(0.0);
		double y = double(0.0);
		double z = double(0.0);

		vector3() { }
		vector3(double x, double y, double z);
		vector3(double value[3]);

		double squared_distance_to(const vector3& right) const {
			double dx = right.x - x;
			double dy = right.y - y;
			double dz = right.z - z;
			return dx * dx + dy * dy + dz * dz;
		}

		double distance_to(const vector3& right) const {
			return std::sqrt(squared_distance_to(right));
		}

		double length() const {
			return sqrt(x * x + y * y + z * z);
		}

		double max() const {
			return std::max(std::max(x, y), z);
		}

		std::string to_string() const;
		std::string to_json() const;

		static vector3 parse_min(const json& bounding_box_json);
		static vector3 parse_max(const json& bounding_box_json);

		vector3 operator-(const vector3& right) const {
			return vector3(x - right.x, y - right.y, z - right.z);
		}

		vector3 operator+(const vector3& right) const {
			return vector3(x + right.x, y + right.y, z + right.z);
		}

		vector3 operator+(const double& scalar) const {
			return vector3(x + scalar, y + scalar, z + scalar);
		}

		vector3 operator/(const double& scalar) const {
			return vector3(x / scalar, y / scalar, z / scalar);
		}

		vector3 operator*(const vector3& right) const {
			return vector3(x * right.x, y * right.y, z * right.z);
		}

		vector3 operator*(const double& scalar) const {
			return vector3(x * scalar, y * scalar, z * scalar);
		}
	
	private:
		static vector3 parse(const json& bounding_box_json, const std::string& field);

	};

}
