#pragma once

#include <mutex>
#include <unordered_map>
#include <deque>
#include <fstream>
#include "common/options.h"
#include "node.h"
#include "chunk.h"

namespace potree {

  struct hierarchy {
    static const int DEFAULT_STEP_SIZE = 4;
    int64_t m_first_chunk_size = 0;
    int64_t m_step_size = 0;
    std::vector<uint8_t> m_buffer;
  };

  struct hierarchy_builder {
  public:
    hierarchy_builder(const std::string& path, int step_size);
    void build();
  private:
    std::shared_ptr<node_batch> m_root_batch;
    std::string m_path;
    int m_step_size = 0;
  
    std::shared_ptr<node_batch> load_batch(const std::string& path);
    std::shared_ptr<buffer> serialize_batch(std::shared_ptr<node_batch> batch, int64_t bytes_written);
    void process_batch(std::shared_ptr<node_batch> batch);
  };

  struct hierarchy_flusher {
  public:
    hierarchy_flusher(const std::string& path);

    void clear();
    void flush(int step_size);
    void write(node* n, int step_size);
    void write(std::vector<node>& nodes, int step_size);

  private:
    std::mutex m_mtx;
    std::string m_path;
    std::unordered_map<std::string, int> m_chunks;
    std::vector<node> m_buffer;

  };

  struct hierarchy_indexer;

  struct hierarchy_writer {
  public:
    hierarchy_writer(const std::shared_ptr<hierarchy_indexer>& indexer);
    void write_and_unload(const std::shared_ptr<potree::node>& node);
    void close_and_wait();
    int64_t get_backlog_size_mb();
  private:
    std::mutex m_mtx;
    std::mutex m_backlog_mtx;
    std::condition_variable m_close_cv;
    int64_t m_capacity = 16 * 1024 * 1024;

    std::shared_ptr<potree::buffer> m_active_buffer;
    std::deque<std::shared_ptr<potree::buffer>> m_backlog;
    std::shared_ptr<hierarchy_indexer> m_indexer;
    std::fstream m_fs_octree;  

    bool m_close_requested = false;
    bool m_closed = false;

    void launch();
  };

  struct hierarchy_indexer : public std::enable_shared_from_this<hierarchy_indexer> {
  public:
    std::atomic_int64_t m_byte_offset = 0;
    std::atomic_int64_t m_bytes_in_memory = 0;
		std::atomic_int64_t m_bytes_to_write = 0;
		std::atomic_int64_t m_bytes_written = 0;
    attributes m_attributes;
    options m_options;

    hierarchy_indexer(const std::string& target_dir);
    ~hierarchy_indexer();

    void wait_for_backlog_below(int max_mb);
    void wait_for_memory_below(int max_mb);
    std::string build_metadata(const options& opts, const std::shared_ptr<status>& state, const hierarchy& hry);
    potree::node gather_chunks(const std::shared_ptr<potree::node>& start, int levels);
    std::vector<potree::node> gather_hierarchy_chunks(const std::shared_ptr<potree::node>& root, int step_size);
    hierarchy create_hiearchy(const std::string& path);
    void flush(const std::shared_ptr<potree::node>& chunk_root);
    void reload();
    std::vector<chunk_node> process_chunk_roots();

    std::string get_target_dir() const { return m_target_dir; }

  private:
    std::mutex m_mtx;
    std::mutex m_root_mtx;
  
    double m_scale = 0.001;
    double m_spacing = 1.0;
    std::atomic_int64_t m_dbg = 0;
    std::unique_ptr<hierarchy_writer> m_writer;
    std::shared_ptr<hierarchy_flusher> m_flusher;

    std::string m_target_dir;
    std::shared_ptr<node> m_root;
    std::vector<std::shared_ptr<node>> m_detached_parts;
    std::vector<node_flush_info> m_flushed_chunk_roots;
    std::fstream m_fs_chunk_roots;

  };

}