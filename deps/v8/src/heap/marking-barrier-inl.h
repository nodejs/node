// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_BARRIER_INL_H_
#define V8_HEAP_MARKING_BARRIER_INL_H_

#include "src/base/logging.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/marking.h"

namespace v8 {
namespace internal {

template <typename TSlot>
void MarkingBarrier::Write(Tagged<HeapObject> host, TSlot slot,
                           Tagged<HeapObject> value) {
  DCHECK(IsCurrentMarkingBarrier(host));
  DCHECK(is_activated_ || shared_heap_worklists_.has_value());
  DCHECK(MemoryChunk::FromHeapObject(host)->IsMarking());

  MarkValue(host, value);

  if (slot.address() && IsCompacting(host)) {
    MarkCompactCollector::RecordSlot(host, slot, value);
  }
}

void MarkingBarrier::MarkValue(Tagged<HeapObject> host,
                               Tagged<HeapObject> value) {
  if (HeapLayout::InReadOnlySpace(value)) return;

  DCHECK(IsCurrentMarkingBarrier(host));
  DCHECK(is_activated_ || shared_heap_worklists_.has_value());

  // When shared heap isn't enabled all objects are local, we can just run the
  // local marking barrier. Also from the point-of-view of the shared space
  // isolate (= main isolate) also shared objects are considered local.
  if (V8_UNLIKELY(uses_shared_heap_) && !is_shared_space_isolate_) {
    // Check whether incremental marking is enabled for that object's space.
    if (!MemoryChunk::FromHeapObject(host)->IsMarking()) {
      return;
    }

    if (v8_flags.black_allocated_pages &&
        HeapLayout::InBlackAllocatedPage(value)) {
      return;
    }

    if (HeapLayout::InWritableSharedSpace(host)) {
      // Invoking shared marking barrier when storing into shared objects.
      MarkValueShared(value);
      return;
    } else if (HeapLayout::InWritableSharedSpace(value)) {
      // No marking needed when storing shared objects in local objects.
      return;
    }
  }

  DCHECK_IMPLIES(HeapLayout::InWritableSharedSpace(host),
                 is_shared_space_isolate_);
  DCHECK_IMPLIES(HeapLayout::InWritableSharedSpace(value),
                 is_shared_space_isolate_);

  DCHECK(is_activated_);
  MarkValueLocal(value);
}

void MarkingBarrier::MarkValueShared(Tagged<HeapObject> value) {
  // Value is either in read-only space or shared heap.
  DCHECK(HeapLayout::InAnySharedSpace(value));

  // We should only reach this on client isolates (= worker isolates).
  DCHECK(!is_shared_space_isolate_);
  DCHECK(shared_heap_worklists_.has_value());

  // Mark shared object and push it onto shared heap worklist.
  if (marking_state_.TryMark(value)) {
    shared_heap_worklists_->Push(value);
  }
}

void MarkingBarrier::MarkValueLocal(Tagged<HeapObject> value) {
  DCHECK(!HeapLayout::InReadOnlySpace(value));
  if (is_minor()) {
    // We do not need to insert into RememberedSet<OLD_TO_NEW> here because the
    // C++ marking barrier already does this for us.
    // TODO(v8:13012): Consider updating C++ barriers to respect
    // POINTERS_TO_HERE_ARE_INTERESTING and POINTERS_FROM_HERE_ARE_INTERESTING
    // page flags and make the following branch a DCHECK.
    if (HeapLayout::InYoungGeneration(value)) {
      MarkingHelper::TryMarkAndPush(
          heap_, current_worklists_.get(), &marking_state_,
          MarkingHelper::WorklistTarget::kRegular, value);
    }
  } else {
    // At this point `ShouldMarkObject()` should always succeed here because
    // value has gone through all the necessary filters. However, we do want to
    // push to the right target worklist immediately.
    const auto target_worklist = MarkingHelper::ShouldMarkObject(heap_, value);
    if (!target_worklist) return;
    MarkingHelper::TryMarkAndPush(heap_, current_worklists_.get(),
                                  &marking_state_, target_worklist.value(),
                                  value);
  }
}

template <typename TSlot>
inline void MarkingBarrier::MarkRange(Tagged<HeapObject> host, TSlot start,
                                      TSlot end) {
  auto* isolate = heap_->isolate();
  const bool record_slots =
      IsCompacting(host) &&
      !MemoryChunk::FromHeapObject(host)->ShouldSkipEvacuationSlotRecording();
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object = slot.Relaxed_Load();
    Tagged<HeapObject> heap_object;
    // Mark both, weak and strong edges.
    if (object.GetHeapObject(isolate, &heap_object)) {
      MarkValue(host, heap_object);
      if (record_slots) {
        major_collector_->RecordSlot(host, HeapObjectSlot(slot), heap_object);
      }
    }
  }
}

bool MarkingBarrier::IsCompacting(Tagged<HeapObject> object) const {
  if (is_compacting_) {
    DCHECK(is_major());
    return true;
  }

  return shared_heap_worklists_.has_value() &&
         HeapLayout::InWritableSharedSpace(object);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_INL_H_
