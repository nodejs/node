// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_STATE_VALUES_UTILS_H_
#define V8_COMPILER_STATE_VALUES_UTILS_H_

#include <array>

#include "src/common/globals.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/zone/zone-hashmap.h"

namespace v8 {
namespace internal {

class BitVector;

namespace compiler {

class Graph;

class V8_EXPORT_PRIVATE StateValuesCache {
 public:
  explicit StateValuesCache(JSGraph* js_graph);

  Node* GetNodeForValues(Node** values, size_t count,
                         const BitVector* liveness = nullptr,
                         int liveness_offset = 0);

 private:
  static const size_t kMaxInputCount = 8;
  using WorkingBuffer = std::array<Node*, kMaxInputCount>;

  struct NodeKey {
    Node* node;

    explicit NodeKey(Node* node) : node(node) {}
  };

  struct StateValuesKey : public NodeKey {
    // ValueArray - array of nodes ({node} has to be nullptr).
    size_t count;
    SparseInputMask mask;
    Node** values;

    StateValuesKey(size_t count, SparseInputMask mask, Node** values)
        : NodeKey(nullptr), count(count), mask(mask), values(values) {}
  };

  static bool AreKeysEqual(void* key1, void* key2);
  static bool IsKeysEqualToNode(StateValuesKey* key, Node* node);
  static bool AreValueKeysEqual(StateValuesKey* key1, StateValuesKey* key2);

  // Fills {node_buffer}, starting from {node_count}, with {values}, starting
  // at {values_idx}, sparsely encoding according to {liveness}. {node_count} is
  // updated with the new number of inputs in {node_buffer}, and a bitmask of
  // the sparse encoding is returned.
  SparseInputMask::BitMaskType FillBufferWithValues(WorkingBuffer* node_buffer,
                                                    size_t* node_count,
                                                    size_t* values_idx,
                                                    Node** values, size_t count,
                                                    const BitVector* liveness,
                                                    int liveness_offset);

  Node* BuildTree(size_t* values_idx, Node** values, size_t count,
                  const BitVector* liveness, int liveness_offset, size_t level);

  WorkingBuffer* GetWorkingSpace(size_t level);
  Node* GetEmptyStateValues();
  Node* GetValuesNodeFromCache(Node** nodes, size_t count,
                               SparseInputMask mask);

  Graph* graph() { return js_graph_->graph(); }
  CommonOperatorBuilder* common() { return js_graph_->common(); }

  Zone* zone() { return graph()->zone(); }

  JSGraph* js_graph_;
  CustomMatcherZoneHashMap hash_map_;
  ZoneVector<WorkingBuffer> working_space_;  // One working space per level.
  Node* empty_state_values_;
};

class V8_EXPORT_PRIVATE StateValuesAccess {
 public:
  struct TypedNode {
    Node* node;
    MachineType type;
    TypedNode(Node* node, MachineType type) : node(node), type(type) {}
  };

  class V8_EXPORT_PRIVATE iterator {
   public:
    bool operator!=(iterator const& other) const;
    iterator& operator++();
    TypedNode operator*();

    Node* node();
    bool done() const { return current_depth_ < 0; }

    // Returns the number of empty nodes that were skipped over.
    size_t AdvanceTillNotEmpty();

   private:
    friend class StateValuesAccess;

    iterator() : current_depth_(-1) {}
    explicit iterator(Node* node);

    MachineType type();
    void Advance();
    void EnsureValid();

    SparseInputMask::InputIterator* Top();
    void Push(Node* node);
    void Pop();

    static const int kMaxInlineDepth = 8;
    SparseInputMask::InputIterator stack_[kMaxInlineDepth];
    int current_depth_;
  };

  explicit StateValuesAccess(Node* node) : node_(node) {}

  size_t size() const;
  iterator begin() const { return iterator(node_); }
  iterator begin_without_receiver() const {
    return ++begin();  // Skip the receiver.
  }
  iterator begin_without_receiver_and_skip(int n_skips) {
    iterator it = begin_without_receiver();
    while (n_skips > 0 && !it.done()) {
      ++it;
      --n_skips;
    }
    return it;
  }
  iterator end() const { return iterator(); }

 private:
  Node* node_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_STATE_VALUES_UTILS_H_
