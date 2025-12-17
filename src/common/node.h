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
#include "point.h"

using namespace nlohmann;

namespace potree {
  enum class node_type : uint8_t {
    NORMAL = 0,
    LEAF,
    PROXY,
  };

  struct node {
  public:
    std::shared_ptr<node> parent;
    std::vector<std::shared_ptr<node>> children;

    std::string id; // chunk_utils
    std::string name;
    std::shared_ptr<buffer> points;
    std::vector<color> colors;
    vector3 min;
    vector3 max;

    int64_t x = 0; // chunk_utils
    int64_t y = 0; // chunk_utils
    int64_t z = 0; // chunk_utils
    int64_t level = 0; // chunk_utils
    int64_t size = 0; // chunk_utils
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
    node(const std::string& name, const vector3& min, const vector3& max);
    node(const std::string& id, int num_points);

    int64_t get_level() const { return name.size() - 1; }
    vector3 get_center() const { return (min + max) * 0.5; }
    bool compare_distance_to_center(const point& a, const point& b) const;
    void sort_by_distance_to_center(std::vector<point>& points) const;
    bool isLeaf() const;
    void addDescendant(std::shared_ptr<node> descendant);
    void traverse(std::function<void(node*, int)> callback, int level = 0);
    void traversePost(std::function<void(node*)> callback);
    node* find(std::string name);
    std::vector<int64_t_point> get_points(const attributes& attrs) const;

    static attributes parse_attributes(const json& metadata);
    static std::shared_ptr<potree::node> load_hierarchy(const std::string& path, const json& metadata);
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