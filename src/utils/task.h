#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <vector>
#include <memory>
#include <functional>


namespace potree {

  struct task {

  };

  typedef std::function<void(std::shared_ptr<task>)> task_processor;

  class task_pool {
  public:
    task_pool(size_t num_threads, task_processor processor);
    ~task_pool();

    void add(const std::shared_ptr<task>& task);
    void close();
    bool is_done();
    void wait();

  private:
    std::mutex m_mtx;
    size_t m_num_threads = 0;
    std::deque<std::shared_ptr<task>> m_tasks;
    task_processor m_processor;
    std::vector<std::thread> m_threads;
    std::atomic<bool> m_is_closed = false;
    std::atomic<int> m_busy_threads = 0;

    void init();
    void process();
  };
}
