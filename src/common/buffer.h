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

    buffer() { }
    buffer(int64_t size);
    ~buffer();

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

    void write(void* source, int64_t size);

  };
}