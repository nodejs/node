// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/heap/mark-compact.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/page-metadata.h"
#include "src/heap/remembered-set-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/transitions.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::MarkObject(
    Tagged<HeapObject> obj, MarkingHelper::WorklistTarget target_worklist) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap_->Contains(obj));
  MarkingHelper::TryMarkAndPush(heap_, local_marking_worklists_.get(),
                                marking_state_, target_worklist, obj);
}

void MarkCompactCollector::MarkRootObject(
    Root root, Tagged<HeapObject> obj,
    MarkingHelper::WorklistTarget target_worklist) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap_->Contains(obj));
  MarkingHelper::TryMarkAndPush(heap_, local_marking_worklists_.get(),
                                marking_state_, target_worklist, obj);
  if (V8_UNLIKELY(in_conservative_stack_scanning_)) {
    DCHECK_EQ(root, Root::kStackRoots);
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
    auto* metadata = MutablePageMetadata::cast(chunk->Metadata());
    if (chunk->IsEvacuationCandidate()) {
      DCHECK(!chunk->InYoungGeneration());
      ReportAbortedEvacuationCandidateDueToFlags(PageMetadata::cast(metadata));
    } else if (chunk->InYoungGeneration() && !chunk->IsLargePage()) {
      DCHECK(chunk->IsToPage());
      if (!metadata->is_quarantined()) {
        metadata->set_is_quarantined(true);
      }
    }
  }
}

// static
template <typename THeapObjectSlot, RecordYoungSlot kRecordYoung>
void MarkCompactCollector::RecordSlot(Tagged<HeapObject> host,
                                      THeapObjectSlot slot,
                                      Tagged<HeapObject> value) {
  MemoryChunk* host_chunk = MemoryChunk::FromHeapObject(host);
  if (host_chunk->ShouldSkipEvacuationSlotRecording()) {
    return;
  }
  RecordSlot<THeapObjectSlot, kRecordYoung>(host_chunk, slot, value);
}

// static
template <typename THeapObjectSlot, RecordYoungSlot kRecordYoung>
void MarkCompactCollector::RecordSlot(MemoryChunk* host_chunk,
                                      THeapObjectSlot slot,
                                      Tagged<HeapObject> value) {
  const MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(value);
  if (!value_chunk->IsEvacuationCandidate() &&
      (!static_cast<bool>(kRecordYoung) ||
       !HeapLayout::InYoungGeneration(value_chunk, value))) {
    return;
  }

  const auto* isolate = Isolate::Current();
  MutablePageMetadata* host_page =
      MutablePageMetadata::cast(host_chunk->Metadata(isolate));
  const MutablePageMetadata* value_page =
      MutablePageMetadata::cast(value_chunk->Metadata(isolate));

  if (static_cast<bool>(kRecordYoung) &&
      HeapLayout::InYoungGeneration(value_chunk, value)) {
    RememberedSet<OLD_TO_NEW_BACKGROUND>::Insert<AccessMode::ATOMIC>(
        host_page, host_chunk->Offset(slot.address()));
  } else if (value_page->is_executable()) {
    DCHECK(OutsideSandbox(value_chunk->address()));
    RememberedSet<TRUSTED_TO_CODE>::Insert<AccessMode::ATOMIC>(
        host_page, host_chunk->Offset(slot.address()));
  } else if (host_page->is_trusted() && value_page->is_trusted()) {
    DCHECK(OutsideSandbox(value_chunk->address()));
    RememberedSet<TRUSTED_TO_TRUSTED>::Insert<AccessMode::ATOMIC>(
        host_page, host_chunk->Offset(slot.address()));
  } else if (V8_LIKELY(!value_page->is_writable_shared()) ||
             host_page->heap()->isolate()->is_shared_space_isolate()) {
    DCHECK_EQ(host_page->heap(), value_page->heap());
    RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(
        host_page, host_chunk->Offset(slot.address()));
  } else {
    // The only case that we do not record are local->shared references from
    // client heaps, see the following DCHECKs.
    DCHECK(!host_page->heap()->isolate()->is_shared_space_isolate());
    DCHECK(value_page->heap()->isolate()->is_shared_space_isolate());
    DCHECK(value_page->is_writable_shared());
    DCHECK_EQ(value_page->is_writable_shared(),
              value_chunk->InWritableSharedSpace());
  }
}

void MarkCompactCollector::AddTransitionArray(Tagged<TransitionArray> array) {
  local_weak_objects()->transition_arrays_local.Push(array);
}

// static
bool MarkCompactCollector::IsOnEvacuationCandidate(Tagged<MaybeObject> obj) {
  return MemoryChunk::FromAddress(obj.ptr())->IsEvacuationCandidate();
}

void RootMarkingVisitor::VisitRootPointer(Root root, const char* description,
                                          FullObjectSlot p) {
  DCHECK(!MapWord::IsPacked(p.Relaxed_Load().ptr()));
  MarkObjectByPointer(root, p);
}

void RootMarkingVisitor::VisitRootPointers(Root root, const char* description,
                                           FullObjectSlot start,
                                           FullObjectSlot end) {
  for (FullObjectSlot p = start; p < end; ++p) {
    MarkObjectByPointer(root, p);
  }
}

void RootMarkingVisitor::MarkObjectByPointer(Root root, FullObjectSlot p) {
  Tagged<Object> object = *p;
#ifdef V8_ENABLE_DIRECT_HANDLE
  if (object.ptr() == kTaggedNullAddress) return;
#endif
  if (!IsHeapObject(object)) return;
  Tagged<HeapObject> heap_object = Cast<HeapObject>(object);
  const auto target_worklist =
      MarkingHelper::ShouldMarkObject(collector_->heap(), heap_object);
  if (!target_worklist) {
    return;
  }
  collector_->MarkRootObject(root, heap_object, target_worklist.value());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
