#include "file_utils.h"
#include <fstream>
#include <filesystem>

#if defined(__linux__)
constexpr auto fseek_64_all_platforms = fseeko64;
#elif defined(WIN32)
constexpr auto fseek_64_all_platforms = _fseeki64;
#elif defined(_WIN32)
constexpr auto fseek_64_all_platforms = _fseeki64;
#endif

using namespace potree;

std::string file_utils::read_text(const std::string& path) {
  std::ifstream t(path);
  std::string str;
  t.seekg(0, std::ios::end);
  str.reserve(t.tellg());
  t.seekg(0, std::ios::beg);
  str.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
  return str;
}

std::shared_ptr<potree::buffer> file_utils::read_binary(const std::string& path) {
  auto file = fopen(path.c_str(), "rb");
  auto size = std::filesystem::file_size(path);
  auto buffer = std::make_shared<potree::buffer>(size);
  fread(buffer->data, 1, size, file);
  fclose(file);
  return buffer;
}

std::vector<uint8_t> file_utils::read_binary(const std::string& path, uint64_t start, uint64_t size) {
  auto file = fopen(path.c_str(), "rb");
  auto total_size = std::filesystem::file_size(path);

  if (start >= total_size) return std::vector<uint8_t>();
  
  if (start + size > total_size) {
    auto clamped_size = total_size - start;
    std::vector<uint8_t> b(clamped_size);
    fseek_64_all_platforms(file, start, SEEK_SET);
    fread(b.data(), 1, clamped_size, file);
    fclose(file);
    return b;
  } 
  else {
    std::vector<uint8_t> b(size);
    fseek_64_all_platforms(file, start, SEEK_SET);
    fread(b.data(), 1, size, file);
    fclose(file);
    return b;
  }
}

void file_utils::read_binary(const std::string& path, uint64_t start, uint64_t size, void* target) {
	auto file = fopen(path.c_str(), "rb");

	auto total_size = std::filesystem::file_size(path);

	if (start >= total_size) return;
	
  if (start + size > total_size) {
		auto clamped_size = total_size - start;
		fseek_64_all_platforms(file, start, SEEK_SET);
		fread(target, 1, clamped_size, file);
		fclose(file);
	} 
  else {
		fseek_64_all_platforms(file, start, SEEK_SET);
		fread(target, 1, size, file);
		fclose(file);
	}

}

json file_utils::read_json(const std::string& path) {
  return json::parse(read_text(path));
}

void file_utils::write_text(const std::string& path, const std::string& text) {
  std::ofstream out;
  out.open(path);
  out << text;
  out.close();
}

size_t file_utils::size(const std::string& file_path) {
  return std::filesystem::file_size(file_path);
}
