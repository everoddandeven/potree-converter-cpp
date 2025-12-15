#include "node.h"
#include "utils/attribute_utils.h"
#include "utils/file_utils.h"

using namespace potree;

node::node(std::string name, vector3 min, vector3 max) {
  this->name = name;
  this->min = min;
  this->max = max;
  children.resize(8, nullptr);
}

bool node::isLeaf() const {
  for (const auto& child : children) {
    if (child != nullptr) {
      return false;
    }
  }

  return true;
}

void node::addDescendant(std::shared_ptr<node> descendant) {
  static std::mutex mtx;
  std::lock_guard<std::mutex> lock(mtx);

  int descendantLevel = descendant->name.size() - 1;

  node* current = this;

  for (int level = 1; level < descendantLevel; level++) {
    int index = descendant->name[level] - '0';

    if (current->children[index] != nullptr) {
      current = current->children[index].get();
    } else {
      std::string childName = current->name + std::to_string(index);
      auto box = bounding_box::child_of(current->min, current->max, index);
      auto child = std::make_shared<node>(childName, box.min, box.max);
      current->children[index] = child;
      current = child.get();
    }
  }

  auto index = descendant->name[descendantLevel] - '0';
  current->children[index] = descendant;
}

void node::traverse(std::function<void(node*, int)> callback, int level)
{
  callback(this, level);

  for (auto child : children) {
    if (child != nullptr) {
      child->traverse(callback, level + 1);
    }
  }
}

void node::traversePost(std::function<void(node*)> callback) {
  for (auto child : children) {

    if (child != nullptr) {
      child->traversePost(callback);
    }
  }

  callback(this);
}

node* node::find(std::string name) {
  node* current = this;
  int depth = name.size() - 1;

  for(int level = 1; level <= depth; level++){
    int index = name.at(level) - '0';
    current = current->children[index].get();
  }

  return current;
}

attributes node::parse_attributes(const json& metadata) {
  std::vector<attribute> attributeList;
  auto jsAttributes = metadata["attributes"];

  for (auto jsAttribute : jsAttributes) {
    std::string name = jsAttribute["name"];
    std::string description = jsAttribute["description"];
    int size = jsAttribute["size"];
    int numElements = jsAttribute["numElements"];
    int elementSize = jsAttribute["elementSize"];
    attribute_type type = attribute_utils::get_type(jsAttribute["type"]);
    attribute attribute(name, size, numElements, elementSize, type);
    attributeList.push_back(attribute);
  }

  double scaleX = metadata["scale"][0];
  double scaleY = metadata["scale"][1];
  double scaleZ = metadata["scale"][2];

  double offsetX = metadata["offset"][0];
  double offsetY = metadata["offset"][1];
  double offsetZ = metadata["offset"][2];

  attributes attrs(attributeList);
  attrs.posScale = { scaleX, scaleY, scaleZ };
  attrs.posOffset = { offsetX, offsetY, offsetZ };

  return attrs;
}

std::shared_ptr<node> node::load_hierarchy(const std::string& path, const json& metadata) {
  auto buffer = file_utils::read_binary(path + "/hierarchy.bin");
  auto hierarchy_md = metadata["hierarchy"];
  int64_t first_chunk_size = metadata["firstChunkSize"];
  int step_size = metadata["stepSize"];

  // load only first hierarchy chunk
  int64_t h_byte_offset = 0;
  int64_t h_byte_size = first_chunk_size;
  int64_t bytes_per_node = 22;
  int64_t num_nodes = buffer->size / bytes_per_node;

  auto root = std::make_shared<node>();
  root->name = "r";
  std::vector<std::shared_ptr<node>> nodes = { root };

  for(int64_t i = 0; i < num_nodes; i++) {
    auto &current = nodes[i];
    node_type type = static_cast<node_type>(buffer->data_u8[i * bytes_per_node + 0]);
    uint8_t child_mask = buffer->data_u8[i * bytes_per_node + 1];
    uint32_t num_points = reinterpret_cast<uint32_t*>(buffer->data_u8 + i * bytes_per_node + 2)[0];
    int64_t byte_offset = reinterpret_cast<uint64_t*>(buffer->data_u8 + i * bytes_per_node + 6)[0];
    int64_t byte_size = reinterpret_cast<uint64_t*>(buffer->data_u8 + i * bytes_per_node + 14)[0];

    if (current->type == node_type::PROXY) {
      // replace proxy with real node
    } else if (type == node_type::PROXY) {
      // load proxy node
      current->numPoints = num_points;
    } else {
      // load real node
      current->byteOffset = byte_offset;
      current->byteSize = byte_size;
      current->numPoints = num_points;  
    }

    current->type = type;

    for(int child_idx = 0; child_idx < 8; child_idx++) {
      bool exists = ((1 << child_idx) & child_mask) != 0;
      if (!exists) continue;

      auto child = std::make_shared<node>();
      child->parent = current;
      child->name = current->name + std::to_string(child_idx);
      current->children[child_idx] = child;
      nodes.push_back(child);
    }
  }

  return root;
}

