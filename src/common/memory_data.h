#pragma once


namespace potree {
  struct memory_data {
    size_t virtual_total = 0;
    size_t virtual_used = 0;
    size_t virtual_usedByProcess = 0;
    size_t virtual_usedByProcess_max = 0;

    size_t physical_total = 0;
    size_t physical_used = 0;
    size_t physical_usedByProcess = 0;
    size_t physical_usedByProcess_max = 0;
  };
}
