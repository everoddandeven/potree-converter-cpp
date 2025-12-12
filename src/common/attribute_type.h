#pragma once

#include <string>

namespace potree {
  enum class attribute_type {
    INT8 = 0,
    INT16 = 1,
    INT32 = 2,
    INT64 = 3,

    UINT8 = 10,
    UINT16 = 11,
    UINT32 = 12,
    UINT64 = 13,

    FLOAT = 20,
    DOUBLE = 21,

    UNDEFINED = 123456,
  };
}