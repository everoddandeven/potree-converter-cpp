#include "hierarchy_builder.h"
#include "utils/string_utils.h"
#include <fstream>
#include <filesystem>

using namespace potree;

hierarchy_builder::hierarchy_builder(const std::string& path, int step_size) {
  m_path = path;
  m_step_size = step_size;
}

std::shared_ptr<node_batch> hierarchy_builder::load_batch(const std::string& m_path) {
  throw std::runtime_error("hierarchy_builder::load_batch(): not implemented");
}

void hierarchy_builder::process_batch(std::shared_ptr<node_batch> batch) {
  throw std::runtime_error("hierarchy_builder::process_batch(): not implemented");
}

std::shared_ptr<buffer> hierarchy_builder::serialize_batch(std::shared_ptr<node_batch> batch, int64_t bytes_written) {
  throw std::runtime_error("hierarchy_builder::serialize_batch(): not implemented");
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
