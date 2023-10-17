// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/common/globals.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/transitions.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::MarkObject(Tagged<HeapObject> host,
                                      Tagged<HeapObject> obj) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap_->Contains(obj));
  if (marking_state_->TryMark(obj)) {
    local_marking_worklists_->Push(obj);
    if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

// static
void MarkCompactCollector::RecordSlot(Tagged<HeapObject> object,
                                      ObjectSlot slot,
                                      Tagged<HeapObject> target) {
  RecordSlot(object, HeapObjectSlot(slot), target);
}

// static
void MarkCompactCollector::RecordSlot(Tagged<HeapObject> object,
                                      HeapObjectSlot slot,
                                      Tagged<HeapObject> target) {
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  if (!source_page->ShouldSkipEvacuationSlotRecording()) {
    RecordSlot(source_page, slot, target);
  }
}

// static
void MarkCompactCollector::RecordSlot(MemoryChunk* source_page,
                                      HeapObjectSlot slot,
                                      Tagged<HeapObject> target) {
  BasicMemoryChunk* target_page = BasicMemoryChunk::FromHeapObject(target);
  if (target_page->IsEvacuationCandidate()) {
    if (target_page->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
      RememberedSet<OLD_TO_CODE>::Insert<AccessMode::ATOMIC>(source_page,
                                                             slot.address());
    } else {
      RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                            slot.address());
    }
  }
}

void MarkCompactCollector::AddTransitionArray(Tagged<TransitionArray> array) {
  local_weak_objects()->transition_arrays_local.Push(array);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
