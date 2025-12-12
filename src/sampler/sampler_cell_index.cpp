#include "sampler_cell_index.h"

using namespace potree;

sampler_cell_index sampler_cell_index::convert(vector3 point, vector3 min, vector3 size, int64_t gridSize) {
  double nx = (point.x - min.x) / size.x;
  double ny = (point.y - min.y) / size.y;
  double nz = (point.z - min.z) / size.z;

  double lx = 2.0 * fmod(double(gridSize) * nx, 1.0) - 1.0;
  double ly = 2.0 * fmod(double(gridSize) * ny, 1.0) - 1.0;
  double lz = 2.0 * fmod(double(gridSize) * nz, 1.0) - 1.0;

  double distance = sqrt(lx * lx + ly * ly + lz * lz);

  int64_t x = double(gridSize) * nx;
  int64_t y = double(gridSize) * ny;
  int64_t z = double(gridSize) * nz;

  x = std::max(int64_t(0), std::min(x, gridSize - 1));
  y = std::max(int64_t(0), std::min(y, gridSize - 1));
  z = std::max(int64_t(0), std::min(z, gridSize - 1));

  int64_t index = x + y * gridSize + z * gridSize * gridSize;

  return { index, distance };
}
