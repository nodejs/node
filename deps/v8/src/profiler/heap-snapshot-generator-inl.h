// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_HEAP_SNAPSHOT_GENERATOR_INL_H_
#define V8_PROFILER_HEAP_SNAPSHOT_GENERATOR_INL_H_

#include "src/profiler/heap-snapshot-generator.h"

namespace v8 {
namespace internal {


HeapEntry* HeapGraphEdge::from() const {
  return &snapshot()->entries()[from_index()];
}


Isolate* HeapGraphEdge::isolate() const {
  return snapshot()->profiler()->isolate();
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

std::deque<HeapGraphEdge*>::iterator HeapEntry::children_begin() {
  DCHECK(children_index_ >= 0);
  SLOW_DCHECK(
      children_index_ < static_cast<int>(snapshot_->children().size()) ||
      (children_index_ == static_cast<int>(snapshot_->children().size()) &&
       children_count_ == 0));
  return snapshot_->children().begin() + children_index_;
}

std::deque<HeapGraphEdge*>::iterator HeapEntry::children_end() {
  return children_begin() + children_count_;
}


Isolate* HeapEntry::isolate() const { return snapshot_->profiler()->isolate(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_HEAP_SNAPSHOT_GENERATOR_INL_H_
