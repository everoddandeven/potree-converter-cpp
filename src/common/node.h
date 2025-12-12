#pragma once

#include <vector>
#include <mutex>
#include <functional>
#include "vector3.h"
#include "cumulative_color.h"
#include "buffer.h"
#include "bounding_box.h"

namespace potree {
  struct node {

    std::vector<std::shared_ptr<node>> children;

    std::string name;
    std::shared_ptr<buffer> points;
    std::vector<cumulative_color> colors;
    vector3 min;
    vector3 max;

    int64_t indexStart = 0;

    int64_t byteOffset = 0;
    int64_t byteSize = 0;
    int64_t numPoints = 0;

    bool sampled = false;

    node() { }

    node(std::string name, vector3 min, vector3 max) {
      this->name = name;
      this->min = min;
      this->max = max;
      children.resize(8, nullptr);
    }

    int64_t level() {
      return name.size() - 1;
    }

    void addDescendant(std::shared_ptr<node> descendant) {
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

    void traverse(std::function<void(node*)> callback) {
      callback(this);

      for (auto child : children) {

        if (child != nullptr) {
          child->traverse(callback);
        }

      }
    }

    void traversePost(std::function<void(node*)> callback) {
      for (auto child : children) {

        if (child != nullptr) {
          child->traversePost(callback);
        }
      }

      callback(this);
    }

    bool isLeaf() {

      for (auto child : children) {
        if (child != nullptr) {
          return false;
        }
      }


      return true;
    }

    node* find(std::string name){

      node* current = this;

      int depth = name.size() - 1;

      for(int level = 1; level <= depth; level++){
        int index = name.at(level) - '0';

        current = current->children[index].get();
      }

      return current;
    }

  };
}