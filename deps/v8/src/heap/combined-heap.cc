// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/combined-heap.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

CombinedHeapObjectIterator::CombinedHeapObjectIterator(
    Heap* heap, HeapObjectIterator::HeapObjectsFiltering filtering)
    : safepoint_scope_(heap),
      heap_iterator_(heap, filtering),
      ro_heap_iterator_(heap->isolate()->read_only_heap()) {}

HeapObject CombinedHeapObjectIterator::Next() {
  HeapObject object = ro_heap_iterator_.Next();
  if (!object.is_null()) {
    return object;
  }
  return heap_iterator_.Next();
}

}  // namespace internal
}  // namespace v8
