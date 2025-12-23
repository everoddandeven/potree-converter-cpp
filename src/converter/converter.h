#pragma once

#include "common/file_source.h"
#include "common/options.h"
#include "geometry/attributes.h"


namespace potree {
  
  struct conversion_stats {
    vector3 m_min = { gen_utils::INF, gen_utils::INF, gen_utils::INF };
    vector3 m_max = { -gen_utils::INF, -gen_utils::INF, -gen_utils::INF };
    int64_t m_total_bytes = 0;
    int64_t m_total_points = 0;

    static conversion_stats compute(const std::vector<file_source>& sources);
  };
  
  struct converter {
  public:
    void convert();
  private:
    options m_options;
    std::shared_ptr<potree::status> m_state;
    void do_chunking(const file_source_container& container, const conversion_stats& stats, attributes& attrs, const std::shared_ptr<gen_utils::monitor>& monitor);
    void do_indexing(const file_source_container& container);
  };
}