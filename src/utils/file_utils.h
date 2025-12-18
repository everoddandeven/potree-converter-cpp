#pragma once

#include <string>
#include "nlohmann/json.hpp"
#include "common/buffer.h"

using namespace nlohmann;

namespace potree {
namespace file_utils {

  std::shared_ptr<buffer> read_binary(const std::string& path);
  std::vector<uint8_t> read_binary(const std::string& path, uint64_t start, uint64_t size);
  void read_binary(const std::string& path, uint64_t start, uint64_t size, void* target);
  json read_json(const std::string& path);
  std::string read_text(const std::string& path);
  void write_text(const std::string& path, const std::string& text);
  size_t size(const std::string& file_path);
}
}