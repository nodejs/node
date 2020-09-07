// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/off-thread-heap.h"

#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/handles/off-thread-transfer-handle-storage-inl.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/roots/roots.h"
#include "src/snapshot/references.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OffThreadHeap::~OffThreadHeap() = default;

OffThreadHeap::OffThreadHeap(Heap* heap)
    : space_(heap),
      lo_space_(heap),
      off_thread_transfer_handles_head_(nullptr) {}

bool OffThreadHeap::Contains(HeapObject obj) {
  return space_.Contains(obj) || lo_space_.Contains(obj);
}

class OffThreadHeap::StringSlotCollectingVisitor : public ObjectVisitor {
 public:
  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    for (ObjectSlot slot = start; slot != end; ++slot) {
      Object obj = *slot;
      if (obj.IsInternalizedString() &&
          !ReadOnlyHeap::Contains(HeapObject::cast(obj))) {
        string_slots.emplace_back(host.address(),
                                  slot.address() - host.address());
      }
    }
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    for (MaybeObjectSlot slot = start; slot != end; ++slot) {
      MaybeObject maybe_obj = *slot;
      HeapObject obj;
      if (maybe_obj.GetHeapObjectIfStrong(&obj)) {
        if (obj.IsInternalizedString() && !ReadOnlyHeap::Contains(obj)) {
          string_slots.emplace_back(host.address(),
                                    slot.address() - host.address());
        }
      }
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override { UNREACHABLE(); }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    UNREACHABLE();
  }

  std::vector<RelativeSlot> string_slots;
};

void OffThreadHeap::FinishOffThread() {
  DCHECK(!is_finished);

  StringSlotCollectingVisitor string_slot_collector;

  // First iterate all objects in the spaces to find string slots. At this point
  // all string slots have to point to off-thread strings or read-only strings.
  {
    PagedSpaceObjectIterator it(&space_);
    for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      obj.IterateBodyFast(&string_slot_collector);
    }
  }
  {
    LargeObjectSpaceObjectIterator it(&lo_space_);
    for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      obj.IterateBodyFast(&string_slot_collector);
    }
  }

  string_slots_ = std::move(string_slot_collector.string_slots);

  OffThreadTransferHandleStorage* storage =
      off_thread_transfer_handles_head_.get();
  while (storage != nullptr) {
    storage->ConvertFromOffThreadHandleOnFinish();
    storage = storage->next();
  }

  is_finished = true;
}

void OffThreadHeap::Publish(Heap* heap) {
  DCHECK(is_finished);
  Isolate* isolate = heap->isolate();
  ReadOnlyRoots roots(isolate);

  // Before we do anything else, ensure that the old-space can expand to the
  // size needed for the off-thread objects. Use capacity rather than size since
  // we're adding entire pages.
  size_t off_thread_size = space_.Capacity() + lo_space_.Size();
  if (!heap->CanExpandOldGeneration(off_thread_size)) {
    heap->CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
    if (!heap->CanExpandOldGeneration(off_thread_size)) {
      heap->FatalProcessOutOfMemory(
          "Can't expand old-space enough to merge off-thread pages.");
    }
  }

  // Merging and transferring handles should be atomic from the point of view
  // of the GC, since we neither want the GC to walk main-thread handles that
  // point into off-thread pages, nor do we want the GC to move the raw
  // pointers we have into off-thread pages before we've had a chance to turn
  // them into real handles.
  // TODO(leszeks): This could be a stronger assertion, that we don't GC at
  // all.
  DisallowHeapAllocation no_gc;

  // Merge the spaces.
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish.Merge");

    heap->old_space()->MergeLocalSpace(&space_);
    heap->lo_space()->MergeOffThreadSpace(&lo_space_);

    DCHECK(heap->CanExpandOldGeneration(0));
  }

  // Transfer all the transfer handles to be real handles. Make sure to do this
  // before creating any handle scopes, to allow these handles to live in the
  // caller's handle scope.
  OffThreadTransferHandleStorage* storage =
      off_thread_transfer_handles_head_.get();
  while (storage != nullptr) {
    storage->ConvertToHandleOnPublish(isolate, &no_gc);
    storage = storage->next();
  }

  // Create a new handle scope after transferring handles, for the slot holder
  // handles below.
  HandleScope handle_scope(isolate);

  // Handlify all the string slot holder objects, so that we can keep track of
  // them if they move.
  //
  // TODO(leszeks): We might be able to create a HandleScope-compatible
  // structure off-thread and merge it into the current handle scope all in
  // one go (DeferredHandles maybe?).
  std::vector<std::pair<Handle<HeapObject>, Handle<Map>>> heap_object_handles;
  std::vector<Handle<Script>> script_handles;
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish.CollectHandles");
    heap_object_handles.reserve(string_slots_.size());
    for (RelativeSlot relative_slot : string_slots_) {
      // TODO(leszeks): Group slots in the same parent object to avoid
      // creating multiple duplicate handles.
      HeapObject obj = HeapObject::FromAddress(relative_slot.object_address);
      heap_object_handles.push_back(
          {handle(obj, isolate), handle(obj.map(), isolate)});

      // De-internalize the string so that we can re-internalize it later.
      String string =
          String::cast(RELAXED_READ_FIELD(obj, relative_slot.slot_offset));
      bool one_byte = string.IsOneByteRepresentation();
      Map map = one_byte ? roots.one_byte_string_map() : roots.string_map();
      string.set_map_no_write_barrier(map);
    }

    script_handles.reserve(script_list_.size());
    for (Script script : script_list_) {
      script_handles.push_back(handle(script, isolate));
    }
  }

  // After this point, all objects are transferred and all handles are valid,
  // so we can GC again.
  no_gc.Release();

  // Possibly trigger a GC if we're close to exhausting the old generation.
  // TODO(leszeks): Adjust the heuristics here.
  heap->StartIncrementalMarkingIfAllocationLimitIsReached(
      heap->GCFlagsForIncrementalMarking(),
      kGCCallbackScheduleIdleGarbageCollection);

  if (!heap->ShouldExpandOldGenerationOnSlowAllocation() ||
      !heap->CanExpandOldGeneration(1 * MB)) {
    heap->CollectGarbage(OLD_SPACE,
                         GarbageCollectionReason::kAllocationFailure);
  }

  // Iterate the string slots, as an offset from the holders we have handles to.
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish.UpdateHandles");
    for (size_t i = 0; i < string_slots_.size(); ++i) {
      HeapObject obj = *heap_object_handles[i].first;
      int slot_offset = string_slots_[i].slot_offset;

      // There's currently no cases where the holder object could have been
      // resized.
      CHECK_EQ(obj.map(), *heap_object_handles[i].second);
      CHECK_LT(slot_offset, obj.Size());

      String string = String::cast(RELAXED_READ_FIELD(obj, slot_offset));
      if (string.IsThinString()) {
        // We may have already internalized this string via another slot.
        String value = ThinString::cast(string).GetUnderlying();
        RELAXED_WRITE_FIELD(obj, slot_offset, value);
        WRITE_BARRIER(obj, slot_offset, value);
      } else {
        HandleScope handle_scope(isolate);

        Handle<String> string_handle = handle(string, isolate);
        Handle<String> internalized_string =
            isolate->factory()->InternalizeString(string_handle);

        DCHECK(string_handle->IsThinString() ||
               string_handle->IsInternalizedString());
        if (*string_handle != *internalized_string) {
          // Re-read the object from the handle in case there was GC during
          // internalization and it moved.
          HeapObject obj = *heap_object_handles[i].first;
          String value = *internalized_string;

          // Sanity checks that the object or string slot value hasn't changed.
          CHECK_EQ(obj.map(), *heap_object_handles[i].second);
          CHECK_LT(slot_offset, obj.Size());
          CHECK_EQ(RELAXED_READ_FIELD(obj, slot_offset), *string_handle);

          RELAXED_WRITE_FIELD(obj, slot_offset, value);
          WRITE_BARRIER(obj, slot_offset, value);
        }
      }
    }

    // Merge the recorded scripts into the isolate's script list.
    // This for loop may seem expensive, but practically there's unlikely to be
    // more than one script in the OffThreadFactory.
    Handle<WeakArrayList> scripts = isolate->factory()->script_list();
    for (Handle<Script> script_handle : script_handles) {
      scripts = WeakArrayList::Append(isolate, scripts,
                                      MaybeObjectHandle::Weak(script_handle));
    }
    heap->SetRootScriptList(*scripts);
  }
}

void OffThreadHeap::AddToScriptList(Handle<Script> shared) {
  script_list_.push_back(*shared);
}

HeapObject OffThreadHeap::AllocateRaw(int size, AllocationType allocation,
                                      AllocationAlignment alignment) {
  DCHECK(!is_finished);

  DCHECK_EQ(allocation, AllocationType::kOld);
  AllocationResult result;
  if (size > kMaxRegularHeapObjectSize) {
    result = lo_space_.AllocateRaw(size);
  } else {
    result = space_.AllocateRaw(size, alignment);
  }
  HeapObject obj = result.ToObjectChecked();
  OnAllocationEvent(obj, size);
  return obj;
}

bool OffThreadHeap::ReserveSpace(Heap::Reservation* reservations) {
#ifdef DEBUG
  for (int space = FIRST_SPACE;
       space < static_cast<int>(SnapshotSpace::kNumberOfHeapSpaces); space++) {
    if (space == OLD_SPACE || space == LO_SPACE) continue;
    Heap::Reservation* reservation = &reservations[space];
    DCHECK_EQ(reservation->size(), 1);
    DCHECK_EQ(reservation->at(0).size, 0);
  }
#endif

  for (auto& chunk : reservations[OLD_SPACE]) {
    int size = chunk.size;
    AllocationResult allocation = space_.AllocateRawUnaligned(size);
    HeapObject free_space = allocation.ToObjectChecked();

    // Mark with a free list node, in case we have a GC before
    // deserializing.
    Address free_space_address = free_space.address();
    CreateFillerObjectAt(free_space_address, size,
                         ClearFreedMemoryMode::kDontClearFreedMemory);
    chunk.start = free_space_address;
    chunk.end = free_space_address + size;
  }

  return true;
}

HeapObject OffThreadHeap::CreateFillerObjectAt(
    Address addr, int size, ClearFreedMemoryMode clear_memory_mode) {
  ReadOnlyRoots roots(this);
  HeapObject filler =
      Heap::CreateFillerObjectAt(roots, addr, size, clear_memory_mode);
  return filler;
}

OffThreadTransferHandleStorage* OffThreadHeap::AddTransferHandleStorage(
    HandleBase handle) {
  DCHECK_IMPLIES(off_thread_transfer_handles_head_ != nullptr,
                 off_thread_transfer_handles_head_->state() ==
                     OffThreadTransferHandleStorage::kOffThreadHandle);
  off_thread_transfer_handles_head_ =
      std::make_unique<OffThreadTransferHandleStorage>(
          handle.location(), std::move(off_thread_transfer_handles_head_));
  return off_thread_transfer_handles_head_.get();
}

}  // namespace internal
}  // namespace v8

// Undefine the heap manipulation macros.
#include "src/objects/object-macros-undef.h"
