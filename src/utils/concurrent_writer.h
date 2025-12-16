#pragma once
#include <unordered_map>
#include <atomic>
#include "gen_utils.h"
#include "common/buffer.h"

namespace potree {

  struct concurrent_writer {
  public:
    concurrent_writer(size_t num_threads, status& state);
    ~concurrent_writer();

    void wait_for_memory_threshold(int64_t threshold);
    void write(const std::string& path, const std::shared_ptr<potree::buffer>& data);
    void join();
  private:
    potree::status m_state;
    std::unordered_map<std::string, std::vector<std::shared_ptr<potree::buffer>>> m_todo;
    std::unordered_map<std::string, int> m_locks;
    std::atomic_int64_t m_bytes_todo = 0;
    std::atomic_int64_t m_bytes_written = 0;
    std::vector<std::thread> m_threads;
    std::mutex m_todo_mtx;
    std::mutex m_join_mtx;
    size_t m_num_threads = 1;
    bool m_join_requested = false;
    double m_t_start = 0;

    void init(status& state);
    void flush_thread();
  };

}
