#include "scale_offset.h"

using namespace potree;

scale_offset scale_offset::compute(const vector3& min, const vector3& max, const vector3& target_scale) {
  vector3 center = (min + max) / 2.0;
	
	// using the center as the origin would be the "right" choice but 
	// it would lead to negative integer coordinates.
	// since the Potree 1.7 release mistakenly interprets the coordinates as uint values,
	// we can't do that and we use 0/0/0 as the bounding box minimum as the origin instead.
	//vector3 offset = center;

	vector3 offset = min;
	vector3 scale = target_scale;
	vector3 size = max - min;

	// we can only use 31 bits because of the int/uint mistake in Potree 1.7
	// And we only use 30 bits to be on the safe sie.
	double min_scale_x = size.x / pow(2.0, 30.0);
	double min_scale_y = size.y / pow(2.0, 30.0);
	double min_scale_z = size.z / pow(2.0, 30.0);

	scale.x = std::max(scale.x, min_scale_x);
	scale.y = std::max(scale.y, min_scale_y);
	scale.z = std::max(scale.z, min_scale_z);

  return { scale, offset };
}