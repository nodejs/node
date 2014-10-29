// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SNAPSHOT_GENERATOR_INL_H_
#define V8_HEAP_SNAPSHOT_GENERATOR_INL_H_

#include "src/heap-snapshot-generator.h"

namespace v8 {
namespace internal {


HeapEntry* HeapGraphEdge::from() const {
  return &snapshot()->entries()[from_index_];
}


HeapSnapshot* HeapGraphEdge::snapshot() const {
  return to_entry_->snapshot();
}


int HeapEntry::index() const {
  return static_cast<int>(this - &snapshot_->entries().first());
}


int HeapEntry::set_children_index(int index) {
  children_index_ = index;
  int next_index = index + children_count_;
  children_count_ = 0;
  return next_index;
}


HeapGraphEdge** HeapEntry::children_arr() {
  DCHECK(children_index_ >= 0);
  SLOW_DCHECK(children_index_ < snapshot_->children().length() ||
      (children_index_ == snapshot_->children().length() &&
       children_count_ == 0));
  return &snapshot_->children().first() + children_index_;
}


} }  // namespace v8::internal

#endif  // V8_HEAP_SNAPSHOT_GENERATOR_INL_H_
