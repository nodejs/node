// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_BARRIER_INL_H_
#define V8_HEAP_MARKING_BARRIER_INL_H_

#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-barrier.h"

namespace v8 {
namespace internal {

bool MarkingBarrier::MarkValue(HeapObject host, HeapObject value) {
  DCHECK(IsCurrentMarkingBarrier());
  DCHECK(is_activated_);
  DCHECK(!marking_state_.IsImpossible(value));
  // Host may have an impossible markbit pattern if manual allocation folding
  // is performed and host happens to be the last word of an allocated region.
  // In that case host has only one markbit and the second markbit belongs to
  // another object. We can detect that case by checking if value is a one word
  // filler map.
  DCHECK(!marking_state_.IsImpossible(host) ||
         value == ReadOnlyRoots(heap_->isolate()).one_pointer_filler_map());
  if (!V8_CONCURRENT_MARKING_BOOL && !marking_state_.IsBlack(host)) {
    // The value will be marked and the slot will be recorded when the marker
    // visits the host object.
    return false;
  }
  if (!ShouldMarkObject(value)) return false;

  if (is_minor()) {
    // We do not need to insert into RememberedSet<OLD_TO_NEW> here because the
    // C++ marking barrier already does this for us.
    if (Heap::InYoungGeneration(value)) {
      WhiteToGreyAndPush(value);  // NEW->NEW
    }
    return false;
  } else {
    if (WhiteToGreyAndPush(value)) {
      if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
        heap_->AddRetainingRoot(Root::kWriteBarrier, value);
      }
    }
    return true;
  }
}

bool MarkingBarrier::ShouldMarkObject(HeapObject object) const {
  if (V8_LIKELY(!uses_shared_heap_)) return true;
  if (v8_flags.shared_space) {
    if (is_shared_heap_isolate_) return true;
    return !object.InSharedHeap();
  } else {
    return is_shared_heap_isolate_ == object.InSharedHeap();
  }
}

template <typename TSlot>
inline void MarkingBarrier::MarkRange(HeapObject host, TSlot start, TSlot end) {
  auto* isolate = heap_->isolate();
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object = slot.Relaxed_Load();
    HeapObject heap_object;
    // Mark both, weak and strong edges.
    if (object.GetHeapObject(isolate, &heap_object)) {
      if (MarkValue(host, heap_object) && is_compacting_) {
        DCHECK(is_major());
        major_collector_->RecordSlot(host, HeapObjectSlot(slot), heap_object);
      }
    }
  }
}

bool MarkingBarrier::WhiteToGreyAndPush(HeapObject obj) {
  if (marking_state_.WhiteToGrey(obj)) {
    current_worklist_->Push(obj);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_INL_H_
