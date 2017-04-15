// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/value-numbering-reducer.h"

#include <cstring>

#include "src/base/functional.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

size_t HashCode(Node* node) {
  size_t h = base::hash_combine(node->op()->HashCode(), node->InputCount());
  for (Node* input : node->inputs()) {
    h = base::hash_combine(h, input->id());
  }
  return h;
}


bool Equals(Node* a, Node* b) {
  DCHECK_NOT_NULL(a);
  DCHECK_NOT_NULL(b);
  DCHECK_NOT_NULL(a->op());
  DCHECK_NOT_NULL(b->op());
  if (!a->op()->Equals(b->op())) return false;
  if (a->InputCount() != b->InputCount()) return false;
  Node::Inputs aInputs = a->inputs();
  Node::Inputs bInputs = b->inputs();

  auto aIt = aInputs.begin();
  auto bIt = bInputs.begin();
  auto aEnd = aInputs.end();

  for (; aIt != aEnd; ++aIt, ++bIt) {
    DCHECK_NOT_NULL(*aIt);
    DCHECK_NOT_NULL(*bIt);
    if ((*aIt)->id() != (*bIt)->id()) return false;
  }
  return true;
}

}  // namespace

ValueNumberingReducer::ValueNumberingReducer(Zone* temp_zone, Zone* graph_zone)
    : entries_(nullptr),
      capacity_(0),
      size_(0),
      temp_zone_(temp_zone),
      graph_zone_(graph_zone) {}

ValueNumberingReducer::~ValueNumberingReducer() {}


Reduction ValueNumberingReducer::Reduce(Node* node) {
  if (!node->op()->HasProperty(Operator::kIdempotent)) return NoChange();

  const size_t hash = HashCode(node);
  if (!entries_) {
    DCHECK(size_ == 0);
    DCHECK(capacity_ == 0);
    // Allocate the initial entries and insert the first entry.
    capacity_ = kInitialCapacity;
    entries_ = temp_zone()->NewArray<Node*>(kInitialCapacity);
    memset(entries_, 0, sizeof(*entries_) * kInitialCapacity);
    entries_[hash & (kInitialCapacity - 1)] = node;
    size_ = 1;
    return NoChange();
  }

  DCHECK(size_ < capacity_);
  DCHECK(size_ + size_ / 4 < capacity_);

  const size_t mask = capacity_ - 1;
  size_t dead = capacity_;

  for (size_t i = hash & mask;; i = (i + 1) & mask) {
    Node* entry = entries_[i];
    if (!entry) {
      if (dead != capacity_) {
        // Reuse dead entry that we discovered on the way.
        entries_[dead] = node;
      } else {
        // Have to insert a new entry.
        entries_[i] = node;
        size_++;

        // Resize to keep load factor below 80%
        if (size_ + size_ / 4 >= capacity_) Grow();
      }
      DCHECK(size_ + size_ / 4 < capacity_);
      return NoChange();
    }

    if (entry == node) {
      // We need to check for a certain class of collisions here. Imagine the
      // following scenario:
      //
      //  1. We insert node1 with op1 and certain inputs at index i.
      //  2. We insert node2 with op2 and certain inputs at index i+1.
      //  3. Some other reducer changes node1 to op2 and the inputs from node2.
      //
      // Now we are called again to reduce node1, and we would return NoChange
      // in this case because we find node1 first, but what we should actually
      // do is return Replace(node2) instead.
      for (size_t j = (i + 1) & mask;; j = (j + 1) & mask) {
        Node* entry = entries_[j];
        if (!entry) {
          // No collision, {node} is fine.
          return NoChange();
        }
        if (entry->IsDead()) {
          continue;
        }
        if (entry == node) {
          // Collision with ourselves, doesn't count as a real collision.
          // Opportunistically clean-up the duplicate entry if we're at the end
          // of a bucket.
          if (!entries_[(j + 1) & mask]) {
            entries_[j] = nullptr;
            size_--;
            return NoChange();
          }
          // Otherwise, keep searching for another collision.
          continue;
        }
        if (Equals(entry, node)) {
          Reduction reduction = ReplaceIfTypesMatch(node, entry);
          if (reduction.Changed()) {
            // Overwrite the colliding entry with the actual entry.
            entries_[i] = entry;
            // Opportunistically clean-up the duplicate entry if we're at the
            // end of a bucket.
            if (!entries_[(j + 1) & mask]) {
              entries_[j] = nullptr;
              size_--;
            }
          }
          return reduction;
        }
      }
    }

    // Skip dead entries, but remember their indices so we can reuse them.
    if (entry->IsDead()) {
      dead = i;
      continue;
    }
    if (Equals(entry, node)) {
      return ReplaceIfTypesMatch(node, entry);
    }
  }
}

Reduction ValueNumberingReducer::ReplaceIfTypesMatch(Node* node,
                                                     Node* replacement) {
  // Make sure the replacement has at least as good type as the original node.
  if (NodeProperties::IsTyped(replacement) && NodeProperties::IsTyped(node)) {
    Type* replacement_type = NodeProperties::GetType(replacement);
    Type* node_type = NodeProperties::GetType(node);
    if (!replacement_type->Is(node_type)) {
      // Ideally, we would set an intersection of {replacement_type} and
      // {node_type} here. However, typing of NumberConstants assigns different
      // types to constants with the same value (it creates a fresh heap
      // number), which would make the intersection empty. To be safe, we use
      // the smaller type if the types are comparable.
      if (node_type->Is(replacement_type)) {
        NodeProperties::SetType(replacement, node_type);
      } else {
        // Types are not comparable => do not replace.
        return NoChange();
      }
    }
  }
  return Replace(replacement);
}


void ValueNumberingReducer::Grow() {
  // Allocate a new block of entries double the previous capacity.
  Node** const old_entries = entries_;
  size_t const old_capacity = capacity_;
  capacity_ *= 2;
  entries_ = temp_zone()->NewArray<Node*>(capacity_);
  memset(entries_, 0, sizeof(*entries_) * capacity_);
  size_ = 0;
  size_t const mask = capacity_ - 1;

  // Insert the old entries into the new block (skipping dead nodes).
  for (size_t i = 0; i < old_capacity; ++i) {
    Node* const old_entry = old_entries[i];
    if (!old_entry || old_entry->IsDead()) continue;
    for (size_t j = HashCode(old_entry) & mask;; j = (j + 1) & mask) {
      Node* const entry = entries_[j];
      if (entry == old_entry) {
        // Skip duplicate of the old entry.
        break;
      }
      if (!entry) {
        entries_[j] = old_entry;
        size_++;
        break;
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
