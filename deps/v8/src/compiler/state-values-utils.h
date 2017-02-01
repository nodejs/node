// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_STATE_VALUES_UTILS_H_
#define V8_COMPILER_STATE_VALUES_UTILS_H_

#include "src/compiler/js-graph.h"

namespace v8 {
namespace internal {

namespace compiler {

class Graph;

class StateValuesCache {
 public:
  explicit StateValuesCache(JSGraph* js_graph);

  Node* GetNodeForValues(Node** values, size_t count);

 private:
  static const size_t kMaxInputCount = 8;

  struct NodeKey {
    Node* node;

    explicit NodeKey(Node* node) : node(node) {}
  };

  struct StateValuesKey : public NodeKey {
    // ValueArray - array of nodes ({node} has to be nullptr).
    size_t count;
    Node** values;

    StateValuesKey(size_t count, Node** values)
        : NodeKey(nullptr), count(count), values(values) {}
  };

  class ValueArrayIterator;

  static bool AreKeysEqual(void* key1, void* key2);
  static bool IsKeysEqualToNode(StateValuesKey* key, Node* node);
  static bool AreValueKeysEqual(StateValuesKey* key1, StateValuesKey* key2);

  Node* BuildTree(ValueArrayIterator* it, size_t max_height);
  NodeVector* GetWorkingSpace(size_t level);
  Node* GetEmptyStateValues();
  Node* GetValuesNodeFromCache(Node** nodes, size_t count);

  Graph* graph() { return js_graph_->graph(); }
  CommonOperatorBuilder* common() { return js_graph_->common(); }

  Zone* zone() { return graph()->zone(); }

  JSGraph* js_graph_;
  CustomMatcherZoneHashMap hash_map_;
  ZoneVector<NodeVector*> working_space_;  // One working space per level.
  Node* empty_state_values_;
};

class StateValuesAccess {
 public:
  struct TypedNode {
    Node* node;
    MachineType type;
    TypedNode(Node* node, MachineType type) : node(node), type(type) {}
  };

  class iterator {
   public:
    // Bare minimum of operators needed for range iteration.
    bool operator!=(iterator& other);
    iterator& operator++();
    TypedNode operator*();

   private:
    friend class StateValuesAccess;

    iterator() : current_depth_(-1) {}
    explicit iterator(Node* node);

    Node* node();
    MachineType type();
    bool done();
    void Advance();

    struct StatePos {
      Node* node;
      int index;

      explicit StatePos(Node* node) : node(node), index(0) {}
      StatePos() {}
    };

    StatePos* Top();
    void Push(Node* node);
    void Pop();

    static const int kMaxInlineDepth = 8;
    StatePos stack_[kMaxInlineDepth];
    int current_depth_;
  };

  explicit StateValuesAccess(Node* node) : node_(node) {}

  size_t size();
  iterator begin() { return iterator(node_); }
  iterator end() { return iterator(); }

 private:
  Node* node_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_STATE_VALUES_UTILS_H_
