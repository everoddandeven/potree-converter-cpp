#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "utils/gen_utils.h"


namespace potree {
  struct buffer {

    void* data = nullptr;
    uint8_t* data_u8 = nullptr;
    uint16_t* data_u16 = nullptr;
    uint32_t* data_u32 = nullptr;
    uint64_t* data_u64 = nullptr;
    int8_t* data_i8 = nullptr;
    int16_t* data_i16 = nullptr;
    int32_t* data_i32 = nullptr;
    int64_t* data_i64 = nullptr;
    float* data_f32 = nullptr;
    double* data_f64 = nullptr;
    char* data_char = nullptr;

    int64_t size = 0;
    int64_t pos = 0;

    buffer() {

    }

    buffer(int64_t size) {
      data = malloc(size);

      if (data == nullptr) {
        auto memory = gen_utils::get_memory_data();

        std::cout << "ERROR: malloc(" << gen_utils::format_number(size) << ") failed." << std::endl;

        auto virtualAvailable = memory.virtual_total - memory.virtual_used;
        auto physicalAvailable = memory.physical_total - memory.physical_used;
        auto GB = 1024.0 * 1024.0 * 1024.0;

        std::cout << "virtual memory(total): " << gen_utils::format_number(double(memory.virtual_total) / GB) << std::endl;
        std::cout << "virtual memory(used): " << gen_utils::format_number(double(memory.virtual_used) / GB, 1) << std::endl;
        std::cout << "virtual memory(available): " << gen_utils::format_number(double(virtualAvailable) / GB, 1) << std::endl;
        std::cout << "virtual memory(used by process): " << gen_utils::format_number(double(memory.virtual_usedByProcess) / GB, 1) << std::endl;
        std::cout << "virtual memory(highest used by process): " << gen_utils::format_number(double(memory.virtual_usedByProcess_max) / GB, 1) << std::endl;

        std::cout << "physical memory(total): " << gen_utils::format_number(double(memory.physical_total) / GB, 1) << std::endl;
        std::cout << "physical memory(available): " << gen_utils::format_number(double(physicalAvailable) / GB, 1) << std::endl;
        std::cout << "physical memory(used): " << gen_utils::format_number(double(memory.physical_used) / GB, 1) << std::endl;
        std::cout << "physical memory(used by process): " << gen_utils::format_number(double(memory.physical_usedByProcess) / GB, 1) << std::endl;
        std::cout << "physical memory(highest used by process): " << gen_utils::format_number(double(memory.physical_usedByProcess_max) / GB, 1) << std::endl;

        std::cout << "also check if there is enough disk space available" << std::endl;

        exit(4312);
      }

      data_u8 = reinterpret_cast<uint8_t*>(data);
      data_u16 = reinterpret_cast<uint16_t*>(data);
      data_u32 = reinterpret_cast<uint32_t*>(data);
      data_u64 = reinterpret_cast<uint64_t*>(data);
      data_i8 = reinterpret_cast<int8_t*>(data);
      data_i16 = reinterpret_cast<int16_t*>(data);
      data_i32 = reinterpret_cast<int32_t*>(data);
      data_i64 = reinterpret_cast<int64_t*>(data);
      data_f32 = reinterpret_cast<float*>(data);
      data_f64 = reinterpret_cast<double*>(data);
      data_char = reinterpret_cast<char*>(data);

      this->size = size;
    }

    ~buffer() {
      free(data);
    }

    template<class T>
    void set(T value, int64_t position) {
      memcpy(data_u8 + position, &value, sizeof(T));
    }

    template<class T>
    T get(int64_t position) {
      T value;

      memcpy(
        &value, 
        data_u8 + position, 
        sizeof(T)
      );

      return value;
    }

    inline void write(void* source, int64_t size) {
      memcpy(data_u8 + pos, source, size);
      pos += size;
    }

  };
}