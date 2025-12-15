#include "task.h"
#include "gen_utils.h"

using namespace potree;

task_pool::task_pool(size_t num_threads, task_processor processor) {
  m_num_threads = num_threads;
  m_processor = processor;

  init();
}

task_pool::~task_pool() {
  close();
}

void task_pool::init() {
  for(int i = 0; i < m_num_threads; i++) {
    m_threads.emplace_back([this]() {
      process();
    });
  }
}

void task_pool::process() {
  while(true) {
    std::shared_ptr<task> task;
    {
      std::lock_guard<std::mutex> lock(m_mtx);
      bool empty = m_tasks.empty();
      bool all_done = empty && m_is_closed;
      bool waiting_for_task = empty && !all_done;
      bool task_available = !empty;

      if (all_done) break;
      else if (task_available) {
        task = m_tasks.front();
        m_tasks.pop_front();

        if (task == nullptr) {
          MWARNING << "task_pool::process(): task is nullptr" << std::endl;
        } else {
          m_busy_threads++;
        }
      }
    }

    if (task != nullptr) {
      MINFO << "task_pool::process(): processing task" << std::endl;
      m_processor(task);
      m_busy_threads--;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool task_pool::is_done() {
  std::lock_guard<std::mutex> lock(m_mtx);
  return m_tasks.empty() && m_busy_threads == 0;
}

void task_pool::add(const std::shared_ptr<task>& task) {
  std::lock_guard<std::mutex> lock(m_mtx);
  m_tasks.push_back(task);
}

void task_pool::wait() {
  size_t size = 0;
  
  for(;;) {
    size = 0;
    {
      std::lock_guard<std::mutex> lock(m_mtx);
      size = m_tasks.size();
    }

    if (size == 0) return;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void task_pool::close() {
  if (m_is_closed) return;
  
  for(auto& t : m_threads) {
    t.join();
  }

  m_is_closed = true;
}
