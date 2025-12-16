#include "point.h"

using namespace potree;

std::pair<int64_t_point, int64_t_point> int64_t_point::compute_box(const std::vector<int64_t_point>& pts) {
  int64_t_point min, max;

  min.x = std::numeric_limits<int64_t>::max();
  min.y = std::numeric_limits<int64_t>::max();
  min.z = std::numeric_limits<int64_t>::max();
  max.x = std::numeric_limits<int64_t>::min();
  max.y = std::numeric_limits<int64_t>::min();
  max.z = std::numeric_limits<int64_t>::min();

  for(const auto& point : pts) {
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);

    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
  }

  return { min, max };
}