// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HEAP_SNAPSHOT_GENERATOR_INL_H_
#define V8_HEAP_SNAPSHOT_GENERATOR_INL_H_

#include "heap-snapshot-generator.h"

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
  ASSERT(children_index_ >= 0);
  SLOW_ASSERT(children_index_ < snapshot_->children().length() ||
      (children_index_ == snapshot_->children().length() &&
       children_count_ == 0));
  return &snapshot_->children().first() + children_index_;
}


SnapshotObjectId HeapObjectsMap::GetNthGcSubrootId(int delta) {
  return kGcRootsFirstSubrootId + delta * kObjectIdStep;
}


HeapObject* V8HeapExplorer::GetNthGcSubrootObject(int delta) {
  return reinterpret_cast<HeapObject*>(
      reinterpret_cast<char*>(kFirstGcSubrootObject) +
      delta * HeapObjectsMap::kObjectIdStep);
}


int V8HeapExplorer::GetGcSubrootOrder(HeapObject* subroot) {
  return static_cast<int>(
      (reinterpret_cast<char*>(subroot) -
       reinterpret_cast<char*>(kFirstGcSubrootObject)) /
      HeapObjectsMap::kObjectIdStep);
}

} }  // namespace v8::internal

#endif  // V8_HEAP_SNAPSHOT_GENERATOR_INL_H_
