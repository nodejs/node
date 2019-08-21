// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_HEAP_INL_H_
#define V8_HEAP_READ_ONLY_HEAP_INL_H_

#include "src/heap/read-only-heap.h"

#include "src/execution/isolate-utils-inl.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

// static
ReadOnlyRoots ReadOnlyHeap::GetReadOnlyRoots(HeapObject object) {
#ifdef V8_SHARED_RO_HEAP
  // This fails if we are creating heap objects and the roots haven't yet been
  // copied into the read-only heap or it has been cleared for testing.
  if (shared_ro_heap_ != nullptr && shared_ro_heap_->init_complete_) {
    return ReadOnlyRoots(shared_ro_heap_->read_only_roots_);
  }
#endif
  return ReadOnlyRoots(GetHeapFromWritableObject(object));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_HEAP_INL_H_
