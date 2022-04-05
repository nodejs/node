// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_INL_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_INL_H_

#include "include/v8-traced-handle.h"
#include "src/base/logging.h"
#include "src/handles/global-handles.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-worklist-inl.h"

namespace v8 {
namespace internal {

class BasicTracedReferenceExtractor {
 public:
  static Address* ObjectReference(const TracedReferenceBase& ref) {
    return const_cast<Address*>(
        reinterpret_cast<const Address*>(ref.GetSlotThreadSafe()));
  }
};

void UnifiedHeapMarkingState::MarkAndPush(
    const TracedReferenceBase& reference) {
  // The following code will crash with null pointer derefs when finding a
  // non-empty `TracedReferenceBase` when `CppHeap` is in detached mode.

  Address* global_handle_location =
      BasicTracedReferenceExtractor::ObjectReference(reference);
  DCHECK_NOT_NULL(global_handle_location);

  GlobalHandles::MarkTraced(global_handle_location);
  Object object(reinterpret_cast<std::atomic<Address>*>(global_handle_location)
                    ->load(std::memory_order_relaxed));
  if (!object.IsHeapObject()) {
    // The embedder is not aware of whether numbers are materialized as heap
    // objects are just passed around as Smis.
    return;
  }
  HeapObject heap_object = HeapObject::cast(object);
  if (marking_state_->WhiteToGrey(heap_object)) {
    local_marking_worklist_->Push(heap_object);
  }
  if (V8_UNLIKELY(track_retaining_path_)) {
    heap_->AddRetainingRoot(Root::kWrapperTracing, heap_object);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_INL_H_
