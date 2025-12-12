#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include "common/buffer.h"

namespace potree {
namespace file_utils {

  static inline std::string read_text(const std::string& path) {
    std::ifstream t(path);
    std::string str;
    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);
    str.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    return str;
  }

  static inline void write_text(const std::string& path, const std::string& text) {
    std::ofstream out;
    out.open(path);
    out << text;
    out.close();
  }

  static inline std::shared_ptr<buffer> read_binary(const std::string& path) {
    auto file = fopen(path.c_str(), "rb");
    auto size = std::filesystem::file_size(path);
    std::shared_ptr<potree::buffer> buffer = std::make_shared<potree::buffer>(size);
    fread(buffer->data, 1, size, file);
    fclose(file);
    return buffer;
  }

}
}