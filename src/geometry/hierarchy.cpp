#include <filesystem>
#include <deque>
#include <condition_variable>
#include <chrono>
#include "common/task.h"
#include "common/buffer.h"
#include "utils/string_utils.h"
#include "utils/file_utils.h"
#include "utils/brotli_utils.h"
#include "hierarchy.h"

using namespace potree;
using namespace std::chrono_literals;

struct load_task : public task {
  std::shared_ptr<potree::node> node;
  int64_t offset;
  int64_t size;

  load_task(const std::shared_ptr<potree::node>& node, int64_t offset, int64_t size) {
    this->node = node;
    this->offset = offset;
    this->size = size;
  }
};

void check_error(const std::shared_ptr<potree::node>& node, int64_t size) {
  if (size >= 0) return;

  MERROR << "invalid call to malloc(" << std::to_string(size) << ")" << std::endl
  << "in function writeAndUnload()" << std::endl
  << "node: " << node->name << std::endl
  << "#points: " << node->numPoints << std::endl
  << "min: " << node->min.to_string() << std::endl
  << "max: " << node->max.to_string() << std::endl;
}

hierarchy_writer::hierarchy_writer(const std::shared_ptr<hierarchy_indexer>& indexer) {
  m_indexer = indexer;
  std::string path = indexer->get_target_dir() + "/octree.bin";
  m_fs_octree.open(path, std::ios::out | std::ios::binary);
  launch();
}

void hierarchy_writer::write_and_unload(const std::shared_ptr<potree::node>& node) {
  if(node->numPoints == 0) return;

  auto attributes = m_indexer->m_attributes;
  std::string encoding = m_indexer->m_options.m_encoding;
  std::shared_ptr<potree::buffer> sourceBuffer;

  if (encoding == "BROTLI") {
    sourceBuffer = brotli_utils::compress(node, attributes);
  } 
  else {
    sourceBuffer = node->points;
  }
  
  int64_t byteSize = sourceBuffer->size;
  node->byteSize = byteSize;
  std::shared_ptr<potree::buffer> buffer = nullptr;
  int64_t targetOffset = 0;
  {
    std::lock_guard<std::mutex> lock(m_mtx);

    int64_t byteOffset = m_indexer->m_byte_offset.fetch_add(byteSize);
    node->byteOffset = byteOffset;

    if (m_active_buffer == nullptr) {
      check_error(node, m_capacity);
      m_active_buffer = std::make_shared<potree::buffer>(m_capacity);
    } 
    else if (m_active_buffer->pos + byteSize > m_capacity) {
      m_backlog.push_back(m_active_buffer);
      m_capacity = std::max(m_capacity, byteSize);
      check_error(node, m_capacity);
      m_active_buffer = std::make_shared<potree::buffer>(m_capacity);
    }

    buffer = m_active_buffer;
    targetOffset = m_active_buffer->pos;
    m_active_buffer->pos += byteSize;
  }	

  memcpy(buffer->data_char + targetOffset, sourceBuffer->data, byteSize);
  node->points = nullptr;
}

void hierarchy_writer::close_and_wait() {
  if (m_closed) return;

  std::unique_lock<std::mutex> lock(m_mtx);

  if (m_active_buffer != nullptr) {
    m_backlog.push_back(m_active_buffer);
  }

  m_close_requested = true;
  m_close_cv.wait(lock);
  m_fs_octree.close();
}

int64_t hierarchy_writer::get_backlog_size_mb() {
  std::lock_guard<std::mutex> lock(m_backlog_mtx);
  int64_t backlogBytes = m_backlog.size() * m_capacity;
  int64_t backlogMB = backlogBytes / (1024 * 1024);
  return backlogMB;
}

void hierarchy_writer::launch() {
  std::thread([&]() {
    for (;;) {
      std::shared_ptr<potree::buffer> buffer = nullptr;

      {
        std::lock_guard<std::mutex> lock(m_mtx);

        if (m_backlog.size() > 0) {
          buffer = m_backlog.front();
          m_backlog.pop_front();
        } 
        else if (m_backlog.size() == 0 && m_close_requested) {
          // DONE! No more work and close requested. quit thread.
          m_close_cv.notify_one();
          break;
        }
      }

      if (buffer != nullptr) {
        int64_t numBytes = buffer->pos;
        m_indexer->m_bytes_written += numBytes;
        m_indexer->m_bytes_to_write -= numBytes;
        m_fs_octree.write(buffer->data_char, numBytes);
        m_indexer->m_bytes_in_memory -= numBytes;
      } 
      else {
        std::this_thread::sleep_for(10ms);
      }
    }
  }).detach();
}


hierarchy_builder::hierarchy_builder(const std::string& path, int step_size) {
  m_path = path;
  m_step_size = step_size;
}

std::shared_ptr<node_batch> hierarchy_builder::load_batch(const std::string& path) {
  std::shared_ptr<potree::buffer> buffer = file_utils::read_binary(path);

  auto batch = std::make_shared<node_batch>();
  batch->path = path;
  batch->name = std::filesystem::path(path).stem().string();
  batch->numNodes = buffer->size / 48;

  // group this batch in chunks of <hierarchyStepSize>
  for(int i = 0; i < batch->numNodes; i++){

    int recordOffset = 48 * i;

    std::string nodeName = std::string(buffer->data_char + recordOffset, 31);
    nodeName = string_utils::replace(nodeName, " ", "");
    nodeName.erase(std::remove(nodeName.begin(), nodeName.end(), ' '), nodeName.end());

    auto node = std::make_shared<potree::node>();
    node->name       = nodeName;
    node->numPoints  = buffer->get<uint32_t>(recordOffset + 31);
    node->byteOffset = buffer->get< int64_t>(recordOffset + 35);
    node->byteSize   = buffer->get< int32_t>(recordOffset + 43);

    // r: 0, r0123: 1, r01230123: 2
    int chunkLevel = (node->name.size() - 2) / 4;
    std::string key = node->name.substr(0, m_step_size * chunkLevel + 1);
    if(node->name == batch->name){
      key = node->name;
    }

    if(batch->chunk_map.find(key) == batch->chunk_map.end()){
      auto chunk = std::make_shared<potree::node>();
      chunk->name = key;
      batch->chunk_map[key] = chunk;
      batch->chunks.push_back(chunk);
    }

    batch->chunk_map[key]->children.push_back(node);
    batch->nodes.push_back(node);
    batch->node_map[node->name] = batch->nodes[batch->nodes.size() - 1];

    bool isChunkKey = ((node->name.size() - 1) % m_step_size) == 0;
    bool isBatchSubChunk = node->name.size() > m_step_size + 1;
    if(isChunkKey && isBatchSubChunk){
      if(batch->chunk_map.find(node->name) == batch->chunk_map.end()){
        auto chunk = std::make_shared<potree::node>();
        chunk->name = node->name;
        batch->chunk_map[node->name] = chunk;
        batch->chunks.push_back(chunk);
      }

      batch->chunk_map[node->name]->children.push_back(node);
    }
  }

  // breadth-first sorted list of chunks
  sort(batch->chunks.begin(), batch->chunks.end(), [](const std::shared_ptr<potree::node>& a, const std::shared_ptr<potree::node>& b) {
    if (a->name.size() != b->name.size()) {
      return a->name.size() < b->name.size();
    } else {
      return a->name < b->name;
    }
  });

  // initialize all nodes as leaf nodes, turn into "normal" if child appears
  // also notify parent that it has a child!
  for(auto node : batch->nodes){
    node->type = node_type::LEAF;
    std::string parentName = node->name.substr(0, node->name.size() - 1);

    auto ptrParent = batch->node_map.find(parentName);

    if(ptrParent != batch->node_map.end()){
      int childIndex = node->name.back() - '0';
      ptrParent->second->type = node_type::NORMAL;
      ptrParent->second->childMask = ptrParent->second->childMask | (1 << childIndex);
    }
    
  }

  // find and flag proxy nodes (pseudo-leaf in one chunk pointing to root of a child-chunk)
  for(auto chunk : batch->chunks){

    //if(chunk->name == batch->name) continue;
    
    auto ptr = batch->node_map.find(chunk->name);

    if(ptr != batch->node_map.end()){
      ptr->second->type = node_type::PROXY;
    }else{
      // could not find a node with the chunk's name
      // should only happen if this chunk's root  
      // is equal to the batch root
      if(chunk->name != batch->name){
        throw std::runtime_error("ERROR: could not find chunk " + chunk->name + " in batch " + batch->name);
      }
    }
  }

  // sort nodes in chunks in breadth-first order
  for(auto chunk : batch->chunks){
    std::sort(chunk->children.begin(), chunk->children.end(), [](const std::shared_ptr<potree::node>& a, const std::shared_ptr<potree::node>& b) {
      if (a->name.size() != b->name.size()) {
        return a->name.size() < b->name.size();
      } else {
        return a->name < b->name;
      }
    });
  }

  return batch;
}

void hierarchy_builder::process_batch(std::shared_ptr<node_batch> batch) {
  // compute byte offsets of chunks relative to batch
  int64_t byteOffset = 0;
  
  for(auto chunk : batch->chunks){
    chunk->byteOffset = byteOffset;

    MINFO << "set offset: " << chunk->name << ", " << chunk->byteOffset << std::endl;

    if(chunk->name != batch->name){
      // this chunk is not the root of the batch.
      // find parent chunk within batch.
      // there must be a leaf node in the parent chunk,
      // which is the proxy node / pointer to this chunk.
      std::string parentName = chunk->name.substr(0, chunk->name.size() - m_step_size);
      if(batch->chunk_map.find(parentName) != batch->chunk_map.end()){
        auto parent = batch->chunk_map[parentName];

        auto proxyNode = batch->node_map[chunk->name];

        if(proxyNode == nullptr){
          throw std::runtime_error("didn't find proxy node " + chunk->name);
        }

        proxyNode->type = node_type::PROXY;
        proxyNode->proxyByteOffset = chunk->byteOffset;
        proxyNode->proxyByteSize = 22 * chunk->children.size();
      } else {
        throw std::runtime_error("ERROR: didn't find chunk " + chunk->name);
      }
    }

    byteOffset += 22 * chunk->children.size();
  }

  batch->byteSize = byteOffset;
}

std::shared_ptr<buffer> hierarchy_builder::serialize_batch(std::shared_ptr<node_batch> batch, int64_t bytes_written) {
  int num_records = 0;

  // all nodes in chunk except chunk root
  for(const auto& chunk : batch->chunks) {
    num_records += chunk->children.size();
  }

  auto buffer = std::make_shared<potree::buffer>(22 * num_records);
  int num_processed = 0;

  for(const auto& chunk : batch->chunks) {
    for(const auto& n : chunk->children) {
      // proxy nodes exist twice - in the chunk and the parent-chunk that points to this chunk
			// only the node in the parent-chunk is a proxy (to its non-proxy counterpart)
      node_type n_type = n->type;
      bool is_proxy = n_type == node_type::PROXY && n->name != chunk->name;
      if (n_type == node_type::PROXY && !is_proxy) {
        n_type = node_type::NORMAL;
      }

      uint64_t byteSize = is_proxy ? n->proxyByteSize : n->byteSize;
      uint64_t byteOffset = (is_proxy ? bytes_written + n->proxyByteOffset : n->byteOffset);
      uint8_t type_t = static_cast<uint8_t>(n_type);

      buffer->set<uint8_t>(type_t        , 22 * num_processed +  0);
      buffer->set<uint8_t>(n->childMask  , 22 * num_processed +  1);
      buffer->set<uint32_t>(n->numPoints , 22 * num_processed +  2);
      buffer->set<uint64_t>(byteOffset   , 22 * num_processed +  6);
      buffer->set<uint64_t>(byteSize     , 22 * num_processed + 14);

      num_processed++;
    }
  }

  batch->byteSize = buffer->size;

  return buffer;
}

void hierarchy_builder::build() {
  if (m_path.empty()) throw std::runtime_error("Cannot build hierarchy: path is empty");
  std::string hierarchyFilePath = m_path + "/../hierarchy.bin";
  std::fstream fout(hierarchyFilePath, std::ios::binary | std::ios::out);
  int64_t bytesWritten = 0;

  auto batch_root = load_batch(m_path + "/r.bin");
  this->m_root_batch = batch_root;

  { // reserve the first <x> bytes in the file for the root chunk
    potree::buffer tmp(22 * batch_root->nodes.size());
    memset(tmp.data, 0, tmp.size);
    fout.write(tmp.data_char, tmp.size);
    bytesWritten = tmp.size;
  }

  // now write all hierarchy batches, except root
  // update proxy nodes in root with byteOffsets of written batches.
  for(auto& entry : std::filesystem::directory_iterator(m_path)){
    auto filepath = entry.path();
    // r0626.txt

    // skip root. it get's special treatment
    if(filepath.filename().string() == "r.bin") continue;
    // skip non *.bin files
    if(!string_utils::iends_with(filepath.string(), ".bin")) continue;

    auto batch = load_batch(filepath.string());

    process_batch(batch);
    auto buffer = serialize_batch(batch, bytesWritten);

    if(batch->nodes.size() > 1){
      auto proxyNode = batch_root->node_map[batch->name];
      proxyNode->type = node_type::PROXY;
      proxyNode->proxyByteOffset = bytesWritten;
      proxyNode->proxyByteSize = 22 * batch->chunk_map[batch->name]->children.size();
      
    } else {
      // if there is only one node in that batch,
      // then we flag that node as leaf in the root-batch
      auto root_batch_node = batch_root->node_map[batch->name];
      root_batch_node->type = node_type::LEAF;
    }

    fout.write(buffer->data_char, buffer->size);
    bytesWritten += buffer->size;
  }

  // close/flush file so that we can reopen it to modify beginning
  fout.close();

  { // update beginning of file with root chunk
    std::fstream f(hierarchyFilePath, std::ios::ate | std::ios::binary | std::ios::out | std::ios::in);
    f.seekg(0);

    auto buffer = serialize_batch(batch_root, 0);

    f.write(buffer->data_char, buffer->size);
    f.close();
  }

  // redundant security check
  if(string_utils::iends_with(this->m_path, ".hierarchyChunks")){
    std::filesystem::remove_all(this->m_path);
  }

  return;
}

hierarchy_flusher::hierarchy_flusher(const std::string& path) {
  m_path = path;
  clear();
}

void hierarchy_flusher::clear() {
  std::filesystem::remove_all(m_path);
  std::filesystem::create_directories(m_path);
}

void hierarchy_flusher::flush(int step_size) {
  std::lock_guard<std::mutex> lock(m_mtx);
  write(m_buffer, step_size);
  m_buffer.clear();
}

void hierarchy_flusher::write(node* n, int step_size) {
  if (n == nullptr) throw std::runtime_error("Cannot write null flushed node");
  std::lock_guard<std::mutex> lock(m_mtx);
  m_buffer.push_back(*n);

  if(m_buffer.size() > 10'000){
    write(m_buffer, step_size);
    m_buffer.clear();
  }
}

// this structure, but guaranteed to be packed
// struct Record{                 size   offset
// 	uint8_t name[31];               31        0
// 	uint32_t numPoints;              4       31
// 	int64_t byteOffset;              8       35
// 	int32_t byteSize;                4       43
// 	uint8_t end = '\n';              1       47
// };                              ===
//                                  48

void hierarchy_flusher::write(std::vector<potree::node>& nodes, int step_size) {
  std::unordered_map<std::string, std::vector<potree::node>> groups;

  for (const auto& node : nodes) {
    std::string key = node.name.substr(0, step_size + 1);
    if(node.name.size() <= step_size + 1){
      key = "r";
    }

    if(groups.find(key) == groups.end()){
      groups[key] = std::vector<potree::node>();
    }

    groups[key].push_back(node);

    // add batch roots to batches (in addition to root batch)
    if(node.name.size() == step_size + 1){
      groups[node.name].push_back(node);
    }
  }

  std::filesystem::create_directories(m_path);

  for(auto [key, groupedNodes] : groups){
    potree::buffer buffer(48 * groupedNodes.size());
    std::stringstream ss;

    for(int i = 0; i < groupedNodes.size(); i++){
      auto node = groupedNodes[i];

      auto name = node.name.c_str();
      memset(buffer.data_u8 + 48 * i, ' ', 31);
      memcpy(buffer.data_u8 + 48 * i, name, node.name.size());
      buffer.set<uint32_t>(node.numPoints,  48 * i + 31);
      buffer.set<uint64_t>(node.byteOffset, 48 * i + 35);
      buffer.set<uint32_t>(node.byteSize,   48 * i + 43);
      buffer.set<char    >('\n',             48 * i + 47);

      ss << string_utils::right_pad(name, 10, ' ') 
        << string_utils::left_pad(std::to_string(node.numPoints), 8, ' ')
        << string_utils::left_pad(std::to_string(node.byteOffset), 12, ' ')
        << string_utils::left_pad(std::to_string(node.byteSize), 12, ' ')
        << std::endl;
    }

    std::string filepath = m_path + "/" + key + ".bin";
    std::fstream fout(filepath, std::ios::app | std::ios::out | std::ios::binary);
    fout.write(buffer.data_char, buffer.size);
    fout.close();

    if(m_chunks.find(key) == m_chunks.end()){
      m_chunks[key] = 0;
    }

    m_chunks[key] += groupedNodes.size();
  }
}

hierarchy_indexer::hierarchy_indexer(const std::string& target_dir) {
  m_target_dir = target_dir;
  m_writer = std::make_unique<hierarchy_writer>(shared_from_this());
  m_flusher = std::make_unique<hierarchy_flusher>(target_dir + "/.hierarchyChunks");
  std::string cr_file = target_dir + "/tmpChunkRoots.bin";
  m_fs_chunk_roots.open(cr_file, std::ios::out | std::ios::binary);
}

hierarchy_indexer::~hierarchy_indexer() {
  m_fs_chunk_roots.close();
}

void hierarchy_indexer::wait_for_backlog_below(int max_mb) {
  while (true) {
    if (m_writer->get_backlog_size_mb() > max_mb) {
      std::this_thread::sleep_for(10ms);
      continue;
    } 
    
    break;
  }
}

void hierarchy_indexer::wait_for_memory_below(int max_mb) {
  while (true) {
    auto memoryData = gen_utils::get_memory_data();
    auto usedMemoryMB = memoryData.virtual_usedByProcess / (1024 * 1024);

    if (usedMemoryMB > max_mb) {
      std::this_thread::sleep_for(10ms);
      continue;
    }

    break;
  }
}

// create vector containing start node and all descendants up to and including levels deeper
// e.g. start 0 and levels 5 -> all nodes from level 0 to inclusive 5.
potree::node hierarchy_indexer::gather_chunks(const std::shared_ptr<potree::node>& start, int levels) {
	int64_t startLevel = start->name.size() - 1;

	potree::node chunk;
	chunk.name = start->name;

	std::vector<std::shared_ptr<potree::node>> stack = { start };
	while (!stack.empty()) {
		auto& node = stack.back();
		stack.pop_back();

		chunk.children.push_back(node);

		int64_t childLevel = node->name.size();
		if (childLevel <= startLevel + levels) {

			for (auto& child : node->children) {
				if (child == nullptr) {
					continue;
				}

				stack.push_back(child);
			}

		}
	}

	return chunk;
}

std::vector<potree::node> hierarchy_indexer::gather_hierarchy_chunks(const std::shared_ptr<potree::node>& root, int step_size) {
  std::vector<potree::node> hierarchyChunks;
	std::vector<std::shared_ptr<potree::node>> stack = { root };
	while (!stack.empty()) {
		auto& chunkRoot = stack.back();
		stack.pop_back();
		auto chunk = gather_chunks(chunkRoot, step_size);

		for (auto& node : chunk.children) {
			bool isProxy = node->get_level() == chunkRoot->get_level() + step_size;

			if (isProxy) {
				stack.push_back(node);
			}
		}

		hierarchyChunks.push_back(chunk);
	}

	return hierarchyChunks;
}

hierarchy hierarchy_indexer::create_hiearchy(const std::string& path) {
	int step_size = hierarchy::DEFAULT_STEP_SIZE;
  // type + childMask + numPoints + offset + size
  constexpr int bytesPerNode = 1 + 1 + 4 + 8 + 8;

	auto chunkSize = [](potree::node& chunk) {
		return chunk.children.size() * bytesPerNode;
	};
  
	auto chunks = gather_hierarchy_chunks(m_root, step_size);

	std::unordered_map<std::string, int> chunkPointers;
	std::vector<int64_t> chunkByteOffsets(chunks.size(), 0);
	int64_t hierarchyBufferSize = 0;
	for (size_t i = 0; i < chunks.size(); i++) {
		auto& chunk = chunks[i];
		chunkPointers[chunk.name] = i;

		node::sort_by_breadth(chunk.children);

		if (i >= 1) {
			chunkByteOffsets[i] = chunkByteOffsets[i - 1] + chunkSize(chunks[i - 1]);
		}

		hierarchyBufferSize += chunkSize(chunk);
	}

	std::vector<uint8_t> hierarchyBuffer(hierarchyBufferSize);

	int offset = 0;
	for (int i = 0; i < chunks.size(); i++) {
		auto& chunk = chunks[i];
		auto chunkLevel = chunk.name.size() - 1;

		for (auto& node : chunk.children) {
			bool isProxy = node->get_level() == chunkLevel + step_size;

			uint8_t childMask = node->get_child_mask();
			uint64_t targetOffset = 0;
			uint64_t targetSize = 0;
			uint32_t numPoints = uint32_t(node->numPoints);
			node_type ntype = node->isLeaf() ? node_type::LEAF : node_type::NORMAL;

      if (isProxy) {
				int targetChunkIndex = chunkPointers[node->name];
				auto targetChunk = chunks[targetChunkIndex];
				ntype = node_type::PROXY;
				targetOffset = chunkByteOffsets[targetChunkIndex];
				targetSize = chunkSize(targetChunk);
			} else {
				targetOffset = node->byteOffset;
				targetSize = node->byteSize;
			}

      uint8_t type = static_cast<uint8_t>(ntype);

			memcpy(hierarchyBuffer.data() + offset + 0, &type, 1);
			memcpy(hierarchyBuffer.data() + offset + 1, &childMask, 1);
			memcpy(hierarchyBuffer.data() + offset + 2, &numPoints, 4);
			memcpy(hierarchyBuffer.data() + offset + 6, &targetOffset, 8);
			memcpy(hierarchyBuffer.data() + offset + 14, &targetSize, 8);

			offset += bytesPerNode;
		}

	}

	hierarchy h;
	h.m_step_size = step_size;
	h.m_buffer = hierarchyBuffer;
	h.m_first_chunk_size = chunks[0].children.size() * bytesPerNode;

	return h; 
}

void hierarchy_indexer::flush(const std::shared_ptr<potree::node>& chunk_root) {
  std::lock_guard<std::mutex> lock(m_root_mtx);

  static int64_t offset = 0;
  int64_t size = m_root->points->size;
  m_fs_chunk_roots.write(m_root->points->data_char, size);

  node_flush_info fcr;
  fcr.m_node = m_root;
  fcr.offset = offset;
  fcr.size = size;

  m_root->points = nullptr;
  m_flushed_chunk_roots.push_back(fcr);
  offset += size;
}

void hierarchy_indexer::reload() {
  gen_utils::profiler pr("hierarchy_indexer::reload()");

  m_fs_chunk_roots.close();

  std::string targetDir = m_target_dir;
  task_pool pool(16, [targetDir](std::shared_ptr<task> t) {
    auto task = std::static_pointer_cast<load_task>(t);
    std::string octreePath = targetDir + "/tmpChunkRoots.bin";
    std::shared_ptr<potree::node> node = task->node;
    int64_t start = task->offset;
    int64_t size = task->size;

    auto buffer = std::make_shared<potree::buffer>(size);
    file_utils::read_binary(octreePath, start, size, buffer->data);

    node->points = buffer;
  });

  for (auto& fcr : m_flushed_chunk_roots) {
    auto task = std::make_shared<load_task>(fcr.m_node, fcr.offset, fcr.size);
    pool.add(task);
  }

  pool.close();
}

std::vector<chunk_node> hierarchy_indexer::process_chunk_roots() {
  std::unordered_map<std::string, std::shared_ptr<chunk_node>> nodesMap;
  std::vector<std::shared_ptr<chunk_node>> nodesList;

  // create/copy nodes
  m_root->traverse([&nodesMap, &nodesList](const std::shared_ptr<potree::node>& node, int level) {
    auto crnode = std::make_shared<chunk_node>();
    crnode->name = node->name;
    crnode->m_node = node;
    crnode->children.resize(node->children.size());

    nodesList.push_back(crnode);
    nodesMap[crnode->name] = crnode;
  });

  // establish hierarchy
  for (auto& crnode : nodesList) {
    std::string parentName = crnode->name.substr(0, crnode->name.size() - 1);

    if (parentName != "") {
      auto parent = nodesMap[parentName];
      int index = crnode->name.at(crnode->name.size() - 1) - '0';

      parent->children[index] = crnode;
    }
  }

  // mark/flag/insert flushed chunk roots
  for(auto& fcr : m_flushed_chunk_roots){
    auto& node = nodesMap[fcr.m_node->name];
    node->m_flushed_roots.push_back(fcr);
    node->numPoints += fcr.m_node->numPoints;
  }

  // recursively merge leaves if sum(points) < threshold
  auto cr_root = nodesMap["r"];
  static int64_t threshold = 5'000'000;

  cr_root->traversePost([](const std::shared_ptr<potree::node>& n) {
    auto node = std::static_pointer_cast<chunk_node>(n);
    if (node->isLeaf()) {
      return;
    }

    int numPoints = 0;
    for(auto child : node->children){
      if(!child) continue;
      numPoints += child->numPoints;
    }

    node->numPoints = numPoints;

    if (node->numPoints < threshold) {
      // merge children into this node
      for (auto& c : node->children) {
        auto child = std::static_pointer_cast<chunk_node>(c);
        if(!child) continue;

        node->m_flushed_roots.insert(node->m_flushed_roots.end(), child->m_flushed_roots.begin(), child->m_flushed_roots.end());
      }

      node->children.clear();
    }
    
  });

  std::vector<chunk_node> tasks;
  auto on_traverse = [&tasks](const std::shared_ptr<potree::node>& n, int level) {
    auto node = std::static_pointer_cast<chunk_node>(n);
    if (node->m_flushed_roots.size() > 0) {
      chunk_node crnode = *node;
      tasks.push_back(crnode);
    }
  };

  cr_root->traverse(on_traverse);

  return tasks;
}
