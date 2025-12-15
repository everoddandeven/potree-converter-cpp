#pragma once

#include <vector>
#include <mutex>
#include <functional>
#include "nlohmann/json.hpp"
#include "vector3.h"
#include "color.h"
#include "buffer.h"
#include "bounding_box.h"
#include "attributes.h"

using namespace nlohmann;

namespace potree {
  enum class node_type : int {
    NORMAL = 0,
    LEAF,
    PROXY,
  };

  struct node {

    std::shared_ptr<node> parent;
    std::vector<std::shared_ptr<node>> children;

    std::string name;
    std::shared_ptr<buffer> points;
    std::vector<color> colors;
    vector3 min;
    vector3 max;

    int64_t indexStart = 0;
    int64_t byteOffset = 0;
    int64_t byteSize = 0;
    uint64_t proxyByteOffset = 0; // hierarchy
		uint64_t proxyByteSize   = 0; // hierarchy

    int64_t numPoints = 0;
    uint8_t childMask = 0; // hierarchy
    node_type type = node_type::PROXY;

    bool sampled = false;

    node() { }
    node(std::string name, vector3 min, vector3 max);

    int64_t level() const { return name.size() - 1; }
    bool isLeaf() const;
    void addDescendant(std::shared_ptr<node> descendant);
    void traverse(std::function<void(node*, int)> callback, int level = 0);
    void traversePost(std::function<void(node*)> callback);
    node* find(std::string name);

    static attributes parse_attributes(const json& metadata);
    static std::shared_ptr<node> load_hierarchy(const std::string& path, const json& metadata);
  };

  struct node_batch {
    std::string name;
    std::string path;
    int numNodes = 0;
    int64_t byteSize = 0;
    std::vector<std::shared_ptr<node>> nodes;
    std::vector<std::shared_ptr<node>> chunks;
    std::unordered_map<std::string, std::shared_ptr<node>> node_map;
    std::unordered_map<std::string, std::shared_ptr<node>> chunk_map;
  };
}