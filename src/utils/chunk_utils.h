#pragma once

#include "common/status.h"
#include "common/chunk.h"
#include "common/buffer.h"
#include "utils/concurrent_writer.h"

namespace potree {
namespace chunk_utils {

  std::shared_ptr<chunks> load_chunks(const std::string& path_in);
  void refine_chunk(const std::shared_ptr<chunk>& chunk, const attributes& attrs);
  void refine(const std::string& target_dir, const status& state);
  std::string build_id(int level, int grid_size, int64_t x, int64_t y, int64_t z);
  void add_buckets(const std::vector<potree::node>& nodes, const std::vector<std::shared_ptr<potree::buffer>>& buckets, const std::shared_ptr<concurrent_writer>& writer, const std::string& target_dir);

}
}