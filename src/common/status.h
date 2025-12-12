#pragma once

#include <string>
#include <map>
#include <mutex>
#include <atomic>


namespace potree {
  struct status {
    std::mutex mtx;
    std::string name = "";
    std::atomic_int64_t pointsTotal = 0;
    std::atomic_int64_t pointsProcessed = 0;
    std::atomic_int64_t bytesProcessed = 0;
    double duration = 0.0;
    std::map<std::string, std::string> values;

    int numPasses = 3;
    int currentPass = 0; // starts with index 1! interval: [1,  numPasses]

    double progress() const {
      return double(pointsProcessed) / double(pointsTotal);
    }
  };
}