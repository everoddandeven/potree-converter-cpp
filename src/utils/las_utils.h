#pragma once

#include <functional>
#include "laszip/laszip_api.h"
#include "geometry/attributes.h"
#include "common/task.h"
#include "las/las_header.h"
#include "gen_utils.h"

namespace potree {
namespace las_utils {

  struct point_count_task : public task {
    std::string path;
    int64_t totalPoints = 0;
    int64_t firstPoint;
    int64_t firstByte;
    int64_t numBytes;
    int64_t numPoints;
    int64_t bpp;
    vector3 scale;
    vector3 offset;
    vector3 min;
    vector3 max;
  };

  class cell_point_counter {
  public:
    
    cell_point_counter(
      const std::vector<file_source>& sources, const vector3& min, const vector3& max,
      int64_t grid_size, const std::shared_ptr<status>& state, attributes& out_attributes, const std::shared_ptr<gen_utils::monitor>& monitor
    );

    std::vector<std::atomic_int32_t> count();

  private:
    std::vector<file_source> m_sources;
    vector3 m_min;
    vector3 m_max;
    std::vector<std::atomic_int32_t> m_grid;
    int64_t m_grid_size;
    attributes m_out_attributes;
    std::shared_ptr<status> m_state;
    std::shared_ptr<gen_utils::monitor> m_monitor;
    std::unique_ptr<task_pool> m_pool;
    task_processor m_processor;
    double m_t_start = 0;

    void init_processor();
    void assembly_sources();
  };

  typedef std::vector<colored_point> point_level;
  typedef std::vector<point_level> point_levels;
  typedef std::function<void(int64_t)> attribute_handler;
  std::vector<attribute> parse_extra_attributes(const las_header& header);
  std::vector<attribute> compute_output_attributes(const las_header& header);
  attributes compute_output_attributes(std::vector<file_source>& sources, std::vector<std::string>& requested_attributes);
  void save(const laszip_header* header, const std::vector<colored_point>& points, const std::string& target);
  void save(const std::string& target, const point_level& points, const vector3& min, const vector3& max);
  void to_laz(const std::string& potree_path);
  std::vector<attribute_handler> create_attribute_handlers(
    laszip_header* header, uint8_t* data, laszip_point* point, 
    attributes& inputAttributes, attributes& outputAttributes
  );
  int process_position(
    const std::string& path, int64_t batch_size, const vector3& scale, 
    const attributes& attrs, attributes& in_attrs, attributes& out_attrs, uint8_t* data, int64_t first_point
  );
}
}