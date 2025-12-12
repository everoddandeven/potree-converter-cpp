#pragma once

#include <vector>

namespace potree {
  struct las_vlr {
    char user_id[16];
    uint16_t record_id = 0;
    uint16_t record_length_after_header = 0;
    char descriptions[32];
    std::vector<uint8_t> data;
  };
}
