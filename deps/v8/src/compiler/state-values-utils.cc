// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/state-values-utils.h"

#include "src/bit-vector.h"

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

  DCHECK(node->opcode() == IrOpcode::kStateValues);
  SparseInputMask node_mask = SparseInputMaskOf(node->op());

  if (node_mask != key->mask) {
    return false;
  }

  // Comparing real inputs rather than sparse inputs, since we already know the
  // sparse input masks are the same.
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
  if (key1->mask != key2->mask) {
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
    empty_state_values_ =
        graph()->NewNode(common()->StateValues(0, SparseInputMask::Dense()));
  }
  return empty_state_values_;
}

StateValuesCache::WorkingBuffer* StateValuesCache::GetWorkingSpace(
    size_t level) {
  if (working_space_.size() <= level) {
    working_space_.resize(level + 1);
  }
  return &working_space_[level];
}

namespace {

int StateValuesHashKey(Node** nodes, size_t count) {
  size_t hash = count;
  for (size_t i = 0; i < count; i++) {
    hash = hash * 23 + (nodes[i] == nullptr ? 0 : nodes[i]->id());
  }
  return static_cast<int>(hash & 0x7fffffff);
}

}  // namespace

Node* StateValuesCache::GetValuesNodeFromCache(Node** nodes, size_t count,
                                               SparseInputMask mask) {
  StateValuesKey key(count, mask, nodes);
  int hash = StateValuesHashKey(nodes, count);
  ZoneHashMap::Entry* lookup =
      hash_map_.LookupOrInsert(&key, hash, ZoneAllocationPolicy(zone()));
  DCHECK_NOT_NULL(lookup);
  Node* node;
  if (lookup->value == nullptr) {
    int node_count = static_cast<int>(count);
    node = graph()->NewNode(common()->StateValues(node_count, mask), node_count,
                            nodes);
    NodeKey* new_key = new (zone()->New(sizeof(NodeKey))) NodeKey(node);
    lookup->key = new_key;
    lookup->value = node;
  } else {
    node = reinterpret_cast<Node*>(lookup->value);
  }
  return node;
}

SparseInputMask::BitMaskType StateValuesCache::FillBufferWithValues(
    WorkingBuffer* node_buffer, size_t* node_count, size_t* values_idx,
    Node** values, size_t count, const BitVector* liveness,
    int liveness_offset) {
  SparseInputMask::BitMaskType input_mask = 0;

  // Virtual nodes are the live nodes plus the implicit optimized out nodes,
  // which are implied by the liveness mask.
  size_t virtual_node_count = *node_count;

  while (*values_idx < count && *node_count < kMaxInputCount &&
         virtual_node_count < SparseInputMask::kMaxSparseInputs) {
    DCHECK_LE(*values_idx, static_cast<size_t>(INT_MAX));

    if (liveness == nullptr ||
        liveness->Contains(liveness_offset + static_cast<int>(*values_idx))) {
      input_mask |= 1 << (virtual_node_count);
      (*node_buffer)[(*node_count)++] = values[*values_idx];
    }
    virtual_node_count++;

    (*values_idx)++;
  }

  DCHECK(*node_count <= StateValuesCache::kMaxInputCount);
  DCHECK(virtual_node_count <= SparseInputMask::kMaxSparseInputs);

  // Add the end marker at the end of the mask.
  input_mask |= SparseInputMask::kEndMarker << virtual_node_count;

  return input_mask;
}

Node* StateValuesCache::BuildTree(size_t* values_idx, Node** values,
                                  size_t count, const BitVector* liveness,
                                  int liveness_offset, size_t level) {
  WorkingBuffer* node_buffer = GetWorkingSpace(level);
  size_t node_count = 0;
  SparseInputMask::BitMaskType input_mask = SparseInputMask::kDenseBitMask;

  if (level == 0) {
    input_mask = FillBufferWithValues(node_buffer, &node_count, values_idx,
                                      values, count, liveness, liveness_offset);
    // Make sure we returned a sparse input mask.
    DCHECK_NE(input_mask, SparseInputMask::kDenseBitMask);
  } else {
    while (*values_idx < count && node_count < kMaxInputCount) {
      if (count - *values_idx < kMaxInputCount - node_count) {
        // If we have fewer values remaining than inputs remaining, dump the
        // remaining values into this node.
        // TODO(leszeks): We could optimise this further by only counting
        // remaining live nodes.

        size_t previous_input_count = node_count;
        input_mask =
            FillBufferWithValues(node_buffer, &node_count, values_idx, values,
                                 count, liveness, liveness_offset);
        // Make sure we have exhausted our values.
        DCHECK_EQ(*values_idx, count);
        // Make sure we returned a sparse input mask.
        DCHECK_NE(input_mask, SparseInputMask::kDenseBitMask);

        // Make sure we haven't touched inputs below previous_input_count in the
        // mask.
        DCHECK_EQ(input_mask & ((1 << previous_input_count) - 1), 0u);
        // Mark all previous inputs as live.
        input_mask |= ((1 << previous_input_count) - 1);

        break;

      } else {
        // Otherwise, add the values to a subtree and add that as an input.
        Node* subtree = BuildTree(values_idx, values, count, liveness,
                                  liveness_offset, level - 1);
        (*node_buffer)[node_count++] = subtree;
        // Don't touch the bitmask, so that it stays dense.
      }
    }
  }

  if (node_count == 1 && input_mask == SparseInputMask::kDenseBitMask) {
    // Elide the StateValue node if there is only one, dense input. This will
    // only happen if we built a single subtree (as nodes with values are always
    // sparse), and so we can replace ourselves with it.
    DCHECK_EQ((*node_buffer)[0]->opcode(), IrOpcode::kStateValues);
    return (*node_buffer)[0];
  } else {
    return GetValuesNodeFromCache(node_buffer->data(), node_count,
                                  SparseInputMask(input_mask));
  }
}

#if DEBUG
namespace {

void CheckTreeContainsValues(Node* tree, Node** values, size_t count,
                             const BitVector* liveness, int liveness_offset) {
  CHECK_EQ(count, StateValuesAccess(tree).size());

  int i;
  auto access = StateValuesAccess(tree);
  auto it = access.begin();
  auto itend = access.end();
  for (i = 0; it != itend; ++it, ++i) {
    if (liveness == nullptr || liveness->Contains(liveness_offset + i)) {
      CHECK((*it).node == values[i]);
    } else {
      CHECK((*it).node == nullptr);
    }
  }
  CHECK_EQ(static_cast<size_t>(i), count);
}

}  // namespace
#endif

Node* StateValuesCache::GetNodeForValues(Node** values, size_t count,
                                         const BitVector* liveness,
                                         int liveness_offset) {
#if DEBUG
  // Check that the values represent actual values, and not a tree of values.
  for (size_t i = 0; i < count; i++) {
    if (values[i] != nullptr) {
      DCHECK_NE(values[i]->opcode(), IrOpcode::kStateValues);
      DCHECK_NE(values[i]->opcode(), IrOpcode::kTypedStateValues);
    }
  }
  if (liveness != nullptr) {
    DCHECK_LE(liveness_offset + count, static_cast<size_t>(liveness->length()));

    for (size_t i = 0; i < count; i++) {
      if (liveness->Contains(liveness_offset + static_cast<int>(i))) {
        DCHECK_NOT_NULL(values[i]);
      }
    }
  }
#endif

  if (count == 0) {
    return GetEmptyStateValues();
  }

  // This is a worst-case tree height estimate, assuming that all values are
  // live. We could get a better estimate by counting zeroes in the liveness
  // vector, but there's no point -- any excess height in the tree will be
  // collapsed by the single-input elision at the end of BuildTree.
  size_t height = 0;
  size_t max_inputs = kMaxInputCount;
  while (count > max_inputs) {
    height++;
    max_inputs *= kMaxInputCount;
  }

  size_t values_idx = 0;
  Node* tree =
      BuildTree(&values_idx, values, count, liveness, liveness_offset, height);
  // The values should be exhausted by the end of BuildTree.
  DCHECK_EQ(values_idx, count);

  // The 'tree' must be rooted with a state value node.
  DCHECK_EQ(tree->opcode(), IrOpcode::kStateValues);

#if DEBUG
  CheckTreeContainsValues(tree, values, count, liveness, liveness_offset);
#endif

  return tree;
}

StateValuesAccess::iterator::iterator(Node* node) : current_depth_(0) {
  stack_[current_depth_] =
      SparseInputMaskOf(node->op()).IterateOverInputs(node);
  EnsureValid();
}

SparseInputMask::InputIterator* StateValuesAccess::iterator::Top() {
  DCHECK(current_depth_ >= 0);
  DCHECK(current_depth_ < kMaxInlineDepth);
  return &(stack_[current_depth_]);
}

void StateValuesAccess::iterator::Push(Node* node) {
  current_depth_++;
  CHECK(current_depth_ < kMaxInlineDepth);
  stack_[current_depth_] =
      SparseInputMaskOf(node->op()).IterateOverInputs(node);
}


void StateValuesAccess::iterator::Pop() {
  DCHECK(current_depth_ >= 0);
  current_depth_--;
}


bool StateValuesAccess::iterator::done() { return current_depth_ < 0; }


void StateValuesAccess::iterator::Advance() {
  Top()->Advance();
  EnsureValid();
}

void StateValuesAccess::iterator::EnsureValid() {
  while (true) {
    SparseInputMask::InputIterator* top = Top();

    if (top->IsEmpty()) {
      // We are on a valid (albeit optimized out) node.
      return;
    }

    if (top->IsEnd()) {
      // We have hit the end of this iterator. Pop the stack and move to the
      // next sibling iterator.
      Pop();
      if (done()) {
        // Stack is exhausted, we have reached the end.
        return;
      }
      Top()->Advance();
      continue;
    }

    // At this point the value is known to be live and within our input nodes.
    Node* value_node = top->GetReal();

    if (value_node->opcode() == IrOpcode::kStateValues ||
        value_node->opcode() == IrOpcode::kTypedStateValues) {
      // Nested state, we need to push to the stack.
      Push(value_node);
      continue;
    }

    // We are on a valid node, we can stop the iteration.
    return;
  }
}

Node* StateValuesAccess::iterator::node() { return Top()->Get(nullptr); }

MachineType StateValuesAccess::iterator::type() {
  Node* parent = Top()->parent();
  if (parent->opcode() == IrOpcode::kStateValues) {
    return MachineType::AnyTagged();
  } else {
    DCHECK_EQ(IrOpcode::kTypedStateValues, parent->opcode());

    if (Top()->IsEmpty()) {
      return MachineType::None();
    } else {
      ZoneVector<MachineType> const* types = MachineTypesOf(parent->op());
      return (*types)[Top()->real_index()];
    }
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
  SparseInputMask mask = SparseInputMaskOf(node_->op());

  SparseInputMask::InputIterator iterator = mask.IterateOverInputs(node_);

  for (; !iterator.IsEnd(); iterator.Advance()) {
    if (iterator.IsEmpty()) {
      count++;
    } else {
      Node* value = iterator.GetReal();
      if (value->opcode() == IrOpcode::kStateValues ||
          value->opcode() == IrOpcode::kTypedStateValues) {
        count += StateValuesAccess(value).size();
      } else {
        count++;
      }
    }
  }

  return count;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
