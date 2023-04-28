// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_BARRIER_INL_H_
#define V8_HEAP_MARKING_BARRIER_INL_H_

#include "src/base/logging.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-barrier.h"

namespace v8 {
namespace internal {

void MarkingBarrier::MarkValue(HeapObject host, HeapObject value) {
  if (value.InReadOnlySpace()) return;

  DCHECK(IsCurrentMarkingBarrier(host));
  DCHECK(is_activated_ || shared_heap_worklist_.has_value());

  DCHECK_IMPLIES(!value.InWritableSharedSpace() || is_shared_space_isolate_,
                 !marking_state_.IsImpossible(value));

  // Host may have an impossible markbit pattern if manual allocation folding
  // is performed and host happens to be the last word of an allocated region.
  // In that case host has only one markbit and the second markbit belongs to
  // another object. We can detect that case by checking if value is a one word
  // filler map.
  DCHECK(!marking_state_.IsImpossible(host) ||
         value == ReadOnlyRoots(heap_->isolate()).one_pointer_filler_map());

  // When shared heap isn't enabled all objects are local, we can just run the
  // local marking barrier. Also from the point-of-view of the shared space
  // isolate (= main isolate) also shared objects are considered local.
  if (V8_UNLIKELY(uses_shared_heap_) && !is_shared_space_isolate_) {
    // Check whether incremental marking is enabled for that object's space.
    if (!MemoryChunk::FromHeapObject(host)->IsMarking()) {
      return;
    }

    if (host.InWritableSharedSpace()) {
      // Invoking shared marking barrier when storing into shared objects.
      MarkValueShared(value);
      return;
    } else if (value.InWritableSharedSpace()) {
      // No marking needed when storing shared objects in local objects.
      return;
    }
  }

  DCHECK_IMPLIES(host.InWritableSharedSpace(), is_shared_space_isolate_);
  DCHECK_IMPLIES(value.InWritableSharedSpace(), is_shared_space_isolate_);

  DCHECK(is_activated_);
  MarkValueLocal(value);
}

void MarkingBarrier::MarkValueShared(HeapObject value) {
  // Value is either in read-only space or shared heap.
  DCHECK(value.InAnySharedSpace());

  // We should only reach this on client isolates (= worker isolates).
  DCHECK(!is_shared_space_isolate_);
  DCHECK(shared_heap_worklist_.has_value());

  // Mark shared object and push it onto shared heap worklist.
  if (marking_state_.TryMark(value)) {
    shared_heap_worklist_->Push(value);
  }
}

void MarkingBarrier::MarkValueLocal(HeapObject value) {
  DCHECK(!value.InReadOnlySpace());
  if (is_minor()) {
    // We do not need to insert into RememberedSet<OLD_TO_NEW> here because the
    // C++ marking barrier already does this for us.
    if (Heap::InYoungGeneration(value)) {
      WhiteToGreyAndPush(value);  // NEW->NEW
    }
  } else {
    if (WhiteToGreyAndPush(value)) {
      if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
        heap_->AddRetainingRoot(Root::kWriteBarrier, value);
      }
    }
  }
}

template <typename TSlot>
inline void MarkingBarrier::MarkRange(HeapObject host, TSlot start, TSlot end) {
  auto* isolate = heap_->isolate();
  const bool record_slots =
      IsCompacting(host) &&
      !MemoryChunk::FromHeapObject(host)->ShouldSkipEvacuationSlotRecording();
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object = slot.Relaxed_Load();
    HeapObject heap_object;
    // Mark both, weak and strong edges.
    if (object.GetHeapObject(isolate, &heap_object)) {
      MarkValue(host, heap_object);
      if (record_slots) {
        major_collector_->RecordSlot(host, HeapObjectSlot(slot), heap_object);
      }
    }
  }
}

bool MarkingBarrier::IsCompacting(HeapObject object) const {
  if (is_compacting_) {
    DCHECK(is_major());
    return true;
  }

  return shared_heap_worklist_.has_value() && object.InWritableSharedSpace();
}

bool MarkingBarrier::WhiteToGreyAndPush(HeapObject obj) {
  if (marking_state_.TryMark(obj)) {
    current_worklist_->Push(obj);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_INL_H_
