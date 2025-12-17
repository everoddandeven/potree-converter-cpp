#include "cell_index.h"

using namespace potree;

cell_index cell_index::convert(vector3 point, vector3 min, vector3 size, int64_t grid_size) {
  double nx = (point.x - min.x) / size.x;
  double ny = (point.y - min.y) / size.y;
  double nz = (point.z - min.z) / size.z;

  double lx = 2.0 * fmod(double(grid_size) * nx, 1.0) - 1.0;
  double ly = 2.0 * fmod(double(grid_size) * ny, 1.0) - 1.0;
  double lz = 2.0 * fmod(double(grid_size) * nz, 1.0) - 1.0;

  double distance = sqrt(lx * lx + ly * ly + lz * lz);

  int64_t x = double(grid_size) * nx;
  int64_t y = double(grid_size) * ny;
  int64_t z = double(grid_size) * nz;

  x = std::max(int64_t(0), std::min(x, grid_size - 1));
  y = std::max(int64_t(0), std::min(y, grid_size - 1));
  z = std::max(int64_t(0), std::min(z, grid_size - 1));

  int64_t index = x + y * grid_size + z * grid_size * grid_size;

  return { index, distance };
}
