// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/state-values-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

StateValuesCache::StateValuesCache(JSGraph* js_graph)
    : js_graph_(js_graph),
      hash_map_(AreKeysEqual, ZoneHashMap::kDefaultHashMapCapacity,
                ZoneAllocationPolicy(zone())),
      working_space_(zone()),
      empty_state_values_(nullptr) {}


// static
bool StateValuesCache::AreKeysEqual(void* key1, void* key2) {
  NodeKey* node_key1 = reinterpret_cast<NodeKey*>(key1);
  NodeKey* node_key2 = reinterpret_cast<NodeKey*>(key2);

  if (node_key1->node == nullptr) {
    if (node_key2->node == nullptr) {
      return AreValueKeysEqual(reinterpret_cast<StateValuesKey*>(key1),
                               reinterpret_cast<StateValuesKey*>(key2));
    } else {
      return IsKeysEqualToNode(reinterpret_cast<StateValuesKey*>(key1),
                               node_key2->node);
    }
  } else {
    if (node_key2->node == nullptr) {
      // If the nodes are already processed, they must be the same.
      return IsKeysEqualToNode(reinterpret_cast<StateValuesKey*>(key2),
                               node_key1->node);
    } else {
      return node_key1->node == node_key2->node;
    }
  }
  UNREACHABLE();
}


// static
bool StateValuesCache::IsKeysEqualToNode(StateValuesKey* key, Node* node) {
  if (key->count != static_cast<size_t>(node->InputCount())) {
    return false;
  }
  for (size_t i = 0; i < key->count; i++) {
    if (key->values[i] != node->InputAt(static_cast<int>(i))) {
      return false;
    }
  }
  return true;
}


// static
bool StateValuesCache::AreValueKeysEqual(StateValuesKey* key1,
                                         StateValuesKey* key2) {
  if (key1->count != key2->count) {
    return false;
  }
  for (size_t i = 0; i < key1->count; i++) {
    if (key1->values[i] != key2->values[i]) {
      return false;
    }
  }
  return true;
}


Node* StateValuesCache::GetEmptyStateValues() {
  if (empty_state_values_ == nullptr) {
    empty_state_values_ = graph()->NewNode(common()->StateValues(0));
  }
  return empty_state_values_;
}


NodeVector* StateValuesCache::GetWorkingSpace(size_t level) {
  while (working_space_.size() <= level) {
    void* space = zone()->New(sizeof(NodeVector));
    working_space_.push_back(new (space)
                                 NodeVector(kMaxInputCount, nullptr, zone()));
  }
  return working_space_[level];
}

namespace {

int StateValuesHashKey(Node** nodes, size_t count) {
  size_t hash = count;
  for (size_t i = 0; i < count; i++) {
    hash = hash * 23 + nodes[i]->id();
  }
  return static_cast<int>(hash & 0x7fffffff);
}

}  // namespace


Node* StateValuesCache::GetValuesNodeFromCache(Node** nodes, size_t count) {
  StateValuesKey key(count, nodes);
  int hash = StateValuesHashKey(nodes, count);
  ZoneHashMap::Entry* lookup =
      hash_map_.LookupOrInsert(&key, hash, ZoneAllocationPolicy(zone()));
  DCHECK_NOT_NULL(lookup);
  Node* node;
  if (lookup->value == nullptr) {
    int input_count = static_cast<int>(count);
    node = graph()->NewNode(common()->StateValues(input_count), input_count,
                            nodes);
    NodeKey* new_key = new (zone()->New(sizeof(NodeKey))) NodeKey(node);
    lookup->key = new_key;
    lookup->value = node;
  } else {
    node = reinterpret_cast<Node*>(lookup->value);
  }
  return node;
}


class StateValuesCache::ValueArrayIterator {
 public:
  ValueArrayIterator(Node** values, size_t count)
      : values_(values), count_(count), current_(0) {}

  void Advance() {
    if (!done()) {
      current_++;
    }
  }

  bool done() { return current_ >= count_; }

  Node* node() {
    DCHECK(!done());
    return values_[current_];
  }

 private:
  Node** values_;
  size_t count_;
  size_t current_;
};


Node* StateValuesCache::BuildTree(ValueArrayIterator* it, size_t max_height) {
  if (max_height == 0) {
    Node* node = it->node();
    it->Advance();
    return node;
  }
  DCHECK(!it->done());

  NodeVector* buffer = GetWorkingSpace(max_height);
  size_t count = 0;
  for (; count < kMaxInputCount; count++) {
    if (it->done()) break;
    (*buffer)[count] = BuildTree(it, max_height - 1);
  }
  if (count == 1) {
    return (*buffer)[0];
  } else {
    return GetValuesNodeFromCache(&(buffer->front()), count);
  }
}


Node* StateValuesCache::GetNodeForValues(Node** values, size_t count) {
#if DEBUG
  for (size_t i = 0; i < count; i++) {
    DCHECK_NE(values[i]->opcode(), IrOpcode::kStateValues);
    DCHECK_NE(values[i]->opcode(), IrOpcode::kTypedStateValues);
  }
#endif
  if (count == 0) {
    return GetEmptyStateValues();
  }
  size_t height = 0;
  size_t max_nodes = 1;
  while (count > max_nodes) {
    height++;
    max_nodes *= kMaxInputCount;
  }

  ValueArrayIterator it(values, count);

  Node* tree = BuildTree(&it, height);

  // If the 'tree' is a single node, equip it with a StateValues wrapper.
  if (tree->opcode() != IrOpcode::kStateValues &&
      tree->opcode() != IrOpcode::kTypedStateValues) {
    tree = GetValuesNodeFromCache(&tree, 1);
  }

  return tree;
}


StateValuesAccess::iterator::iterator(Node* node) : current_depth_(0) {
  // A hacky way initialize - just set the index before the node we want
  // to process and then advance to it.
  stack_[current_depth_].node = node;
  stack_[current_depth_].index = -1;
  Advance();
}


StateValuesAccess::iterator::StatePos* StateValuesAccess::iterator::Top() {
  DCHECK(current_depth_ >= 0);
  DCHECK(current_depth_ < kMaxInlineDepth);
  return &(stack_[current_depth_]);
}


void StateValuesAccess::iterator::Push(Node* node) {
  current_depth_++;
  CHECK(current_depth_ < kMaxInlineDepth);
  stack_[current_depth_].node = node;
  stack_[current_depth_].index = 0;
}


void StateValuesAccess::iterator::Pop() {
  DCHECK(current_depth_ >= 0);
  current_depth_--;
}


bool StateValuesAccess::iterator::done() { return current_depth_ < 0; }


void StateValuesAccess::iterator::Advance() {
  // Advance the current index.
  Top()->index++;

  // Fix up the position to point to a valid node.
  while (true) {
    // TODO(jarin): Factor to a separate method.
    Node* node = Top()->node;
    int index = Top()->index;

    if (index >= node->InputCount()) {
      // Pop stack and move to the next sibling.
      Pop();
      if (done()) {
        // Stack is exhausted, we have reached the end.
        return;
      }
      Top()->index++;
    } else if (node->InputAt(index)->opcode() == IrOpcode::kStateValues ||
               node->InputAt(index)->opcode() == IrOpcode::kTypedStateValues) {
      // Nested state, we need to push to the stack.
      Push(node->InputAt(index));
    } else {
      // We are on a valid node, we can stop the iteration.
      return;
    }
  }
}


Node* StateValuesAccess::iterator::node() {
  return Top()->node->InputAt(Top()->index);
}


MachineType StateValuesAccess::iterator::type() {
  Node* state = Top()->node;
  if (state->opcode() == IrOpcode::kStateValues) {
    return MachineType::AnyTagged();
  } else {
    DCHECK_EQ(IrOpcode::kTypedStateValues, state->opcode());
    const ZoneVector<MachineType>* types =
        OpParameter<const ZoneVector<MachineType>*>(state);
    return (*types)[Top()->index];
  }
}


bool StateValuesAccess::iterator::operator!=(iterator& other) {
  // We only allow comparison with end().
  CHECK(other.done());
  return !done();
}


StateValuesAccess::iterator& StateValuesAccess::iterator::operator++() {
  Advance();
  return *this;
}


StateValuesAccess::TypedNode StateValuesAccess::iterator::operator*() {
  return TypedNode(node(), type());
}


size_t StateValuesAccess::size() {
  size_t count = 0;
  for (int i = 0; i < node_->InputCount(); i++) {
    if (node_->InputAt(i)->opcode() == IrOpcode::kStateValues ||
        node_->InputAt(i)->opcode() == IrOpcode::kTypedStateValues) {
      count += StateValuesAccess(node_->InputAt(i)).size();
    } else {
      count++;
    }
  }
  return count;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
