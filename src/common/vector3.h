#pragma once

#include <string>
#include <cmath>
#include <limits>
#include <sstream>
#include <algorithm>
#include <string>
#include <iomanip>

namespace potree {

	struct vector3 {

		double x = double(0.0);
		double y = double(0.0);
		double z = double(0.0);

		vector3() { }

		vector3(double x, double y, double z) {
			this->x = x;
			this->y = y;
			this->z = z;
		}

		vector3(double value[3]) {
			this->x = value[0];
			this->y = value[1];
			this->z = value[2];
		}

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

		std::string to_string() {
			auto digits = std::numeric_limits<double>::max_digits10;
			std::stringstream ss;
			ss << std::setprecision(digits);
			ss << x << ", " << y << ", " << z;
			return ss.str();
		}

	};

}
