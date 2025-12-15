#include "bounding_box.h"

using namespace potree;

bounding_box::bounding_box() {
	this->min = { gen_utils::INF, gen_utils::INF, gen_utils::INF };
	this->max = { -gen_utils::INF, -gen_utils::INF, -gen_utils::INF };
}

bounding_box::bounding_box(const vector3& min, const vector3& max) {
	this->min = min;
	this->max = max;
}

bounding_box bounding_box::child_of(int index) const {
	return child_of(min, max, index);
}

bounding_box bounding_box::child_of(const vector3& min, const vector3& max, int index) {
	bounding_box box;
	auto size = max - min;
	vector3 center = min + (size * 0.5);

	if ((index & 0b100) == 0) {
		box.min.x = min.x;
		box.max.x = center.x;
	} else {
		box.min.x = center.x;
		box.max.x = max.x;
	}

	if ((index & 0b010) == 0) {
		box.min.y = min.y;
		box.max.y = center.y;
	} else {
		box.min.y = center.y;
		box.max.y = max.y;
	}

	if ((index & 0b001) == 0) {
		box.min.z = min.z;
		box.max.z = center.z;
	} else {
		box.min.z = center.z;
		box.max.z = max.z;
	}

	return box;
}

bounding_box bounding_box::parse(const json& metadata) {
	bounding_box bbox;
	bbox.min = {
		metadata["min"][0].get<double>(),
		metadata["min"][1].get<double>(),
		metadata["min"][2].get<double>()
	};
	bbox.max = {
		metadata["max"][0].get<double>(),
		metadata["max"][1].get<double>(),
		metadata["max"][2].get<double>()
	};

	return bbox;
}