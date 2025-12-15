#pragma once

#include "node.h"

namespace potree {

  struct hierarchy_builder {
  public:
    hierarchy_builder(const std::string& path, int step_size);
    void build();
  private:
    std::shared_ptr<node_batch> m_root_batch;
    std::string m_path;
    int m_step_size = 0;
  
    std::shared_ptr<node_batch> load_batch(const std::string& path);
    std::shared_ptr<buffer> serialize_batch(std::shared_ptr<node_batch> batch, int64_t bytes_written);
    void process_batch(std::shared_ptr<node_batch> batch);
  };

}