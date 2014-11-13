// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/value-numbering-reducer.h"

#include <cstring>

#include "src/base/functional.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

size_t HashCode(Node* node) {
  size_t h = base::hash_combine(node->op()->HashCode(), node->InputCount());
  for (int j = 0; j < node->InputCount(); ++j) {
    h = base::hash_combine(h, node->InputAt(j)->id());
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
  for (int j = 0; j < a->InputCount(); ++j) {
    DCHECK_NOT_NULL(a->InputAt(j));
    DCHECK_NOT_NULL(b->InputAt(j));
    if (a->InputAt(j)->id() != b->InputAt(j)->id()) return false;
  }
  return true;
}

}  // namespace


ValueNumberingReducer::ValueNumberingReducer(Zone* zone)
    : entries_(nullptr), capacity_(0), size_(0), zone_(zone) {}


ValueNumberingReducer::~ValueNumberingReducer() {}


Reduction ValueNumberingReducer::Reduce(Node* node) {
  if (!node->op()->HasProperty(Operator::kEliminatable)) return NoChange();

  const size_t hash = HashCode(node);
  if (!entries_) {
    DCHECK(size_ == 0);
    DCHECK(capacity_ == 0);
    // Allocate the initial entries and insert the first entry.
    capacity_ = kInitialCapacity;
    entries_ = zone()->NewArray<Node*>(kInitialCapacity);
    memset(entries_, 0, sizeof(*entries_) * kInitialCapacity);
    entries_[hash & (kInitialCapacity - 1)] = node;
    size_ = 1;
    return NoChange();
  }

  DCHECK(size_ < capacity_);
  DCHECK(size_ * kCapacityToSizeRatio < capacity_);

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

        // Resize to keep load factor below 1/kCapacityToSizeRatio.
        if (size_ * kCapacityToSizeRatio >= capacity_) Grow();
      }
      DCHECK(size_ * kCapacityToSizeRatio < capacity_);
      return NoChange();
    }
    if (entry == node) {
      return NoChange();
    }

    // Skip dead entries, but remember their indices so we can reuse them.
    if (entry->IsDead()) {
      dead = i;
      continue;
    }
    if (Equals(entry, node)) {
      return Replace(entry);
    }
  }
}


void ValueNumberingReducer::Grow() {
  // Allocate a new block of entries kCapacityToSizeRatio times the previous
  // capacity.
  Node** const old_entries = entries_;
  size_t const old_capacity = capacity_;
  capacity_ *= kCapacityToSizeRatio;
  entries_ = zone()->NewArray<Node*>(static_cast<int>(capacity_));
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
