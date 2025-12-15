#pragma once

#include "common/status.h"
#include "common/chunk.h"

namespace potree {
namespace chunk_utils {

  std::shared_ptr<chunks> load_chunks(const std::string& path_in);
  void refine_chunk(const std::shared_ptr<chunk>& chunk, const attributes& attrs);
  void refine(const std::string& target_dir, const status& state);
}
}