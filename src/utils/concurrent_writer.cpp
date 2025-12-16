#include <fstream>
#include <chrono>
#include "concurrent_writer.h"

using namespace potree;
using namespace std::chrono_literals;

concurrent_writer::concurrent_writer(size_t num_threads, status& state) {
  m_num_threads = num_threads;
  m_t_start = gen_utils::now();
  init(state);
}

concurrent_writer::~concurrent_writer() {
  join();
}

void concurrent_writer::init(status& state) {
  for (int64_t i = 0; i < m_num_threads; i++) {
    m_threads.emplace_back([&]() {
      flush_thread();
    });
  }

  m_threads.emplace_back([&]() {
    for(;;) {
      {
        std::lock_guard<std::mutex> lock_t(m_todo_mtx);
        std::lock_guard<std::mutex> lock_j(m_join_mtx);

        bool nothing_todo = m_todo.empty();

        if (nothing_todo && m_join_requested) return;

        int64_t mb_todo = m_bytes_todo / (1024 * 1024);
        int64_t mb_done = m_bytes_written / (1024 * 1024);

        double duration = gen_utils::now() - m_t_start;
        double throughput = mb_done / duration;

        state.name = "DISTRIBUITING";
      }

      std::this_thread::sleep_for(100ms);
    }
  });
}

void concurrent_writer::join() {
  {
    std::lock_guard<std::mutex> lock(m_join_mtx);
    m_join_requested = true;
  }

  for(auto& t : m_threads) t.join();

  m_threads.clear();
}

void concurrent_writer::write(const std::string& path, const std::shared_ptr<potree::buffer>& data) {
  std::lock_guard<std::mutex> lock(m_todo_mtx);
  m_bytes_todo += data->size;
  m_todo[path].push_back(data);
}

void concurrent_writer::wait_for_memory_threshold(int64_t threshold) {
  while (m_bytes_todo / (1024 * 1024) > threshold) {
    std::this_thread::sleep_for(10ms);
  }
}

void concurrent_writer::flush_thread() {
  for(;;) {
    std::string path = "";
    std::vector<std::shared_ptr<potree::buffer>> work;

    {
      std::lock_guard<std::mutex> lockT(m_todo_mtx);
      std::lock_guard<std::mutex> lockJ(m_join_mtx);

      bool nothingTodo = m_todo.size() == 0;

      if (nothingTodo && m_join_requested) {
        return;
      } 
      else {
        auto it = m_todo.begin();

        while (it != m_todo.end()) {
          std::string path = it->first;

          if (m_locks.find(path) == m_locks.end()) {
            break;
          }

          it++;
        }

        if (it != m_todo.end()) {
          path = it->first;
          work = it->second;

          m_todo.erase(it);
          m_locks[path] = 1;
        }
        
      }
    }

    // if no work available, sleep and try again later
    if (work.size() == 0) {
      std::this_thread::sleep_for(10ms);
      continue;
    } 

    std::fstream fout;
    fout.open(path, std::ios::out | std::ios::app | std::ios::binary);

    for (auto batch : work) {
      fout.write(batch->data_char, batch->size);

      m_bytes_todo -= batch->size;
      m_bytes_written += batch->size;
    }

    fout.close();

    {
      std::lock_guard<std::mutex> lockT(m_todo_mtx);
      std::lock_guard<std::mutex> lockJ(m_join_mtx);

      auto itLocks = m_locks.find(path);
      m_locks.erase(itLocks);
    }

  }

}
