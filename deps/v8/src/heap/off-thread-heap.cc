// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/off-thread-heap.h"

#include "src/heap/spaces-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OffThreadHeap::OffThreadHeap(Heap* heap) : space_(heap), lo_space_(heap) {}

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

  is_finished = true;
}

void OffThreadHeap::Publish(Heap* heap) {
  DCHECK(is_finished);
  Isolate* isolate = heap->isolate();
  ReadOnlyRoots roots(isolate);

  HandleScope handle_scope(isolate);

  // First, handlify all the string slot holder objects, so that we can keep
  // track of them if they move.
  //
  // TODO(leszeks): We might be able to create a HandleScope-compatible
  // structure off-thread and merge it into the current handle scope all in one
  // go (DeferredHandles maybe?).
  std::vector<Handle<HeapObject>> heap_object_handles;
  std::vector<Handle<Script>> script_handles;
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish.CollectHandles");
    heap_object_handles.reserve(string_slots_.size());
    for (RelativeSlot relative_slot : string_slots_) {
      // TODO(leszeks): Group slots in the same parent object to avoid creating
      // multiple duplicate handles.
      HeapObject obj = HeapObject::FromAddress(relative_slot.object_address);
      heap_object_handles.push_back(handle(obj, isolate));

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

  // Then merge the spaces. At this point, we are allowed to point between (no
  // longer) off-thread pages and main-thread heap pages, and objects in the
  // previously off-thread page can move.
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish.Merge");
    Heap* heap = isolate->heap();

    // Ensure that the old-space can expand do the size needed for the
    // off-thread objects. Use capacity rather than size since we're adding
    // entire pages.
    size_t off_thread_size = space_.Capacity() + lo_space_.Size();
    if (!heap->CanExpandOldGeneration(off_thread_size)) {
      heap->InvokeNearHeapLimitCallback();
      if (!heap->CanExpandOldGeneration(off_thread_size)) {
        heap->CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
        if (!heap->CanExpandOldGeneration(off_thread_size)) {
          heap->FatalProcessOutOfMemory(
              "Can't expand old-space enough to merge off-thread pages.");
        }
      }
    }

    heap->old_space()->MergeLocalSpace(&space_);
    heap->lo_space()->MergeOffThreadSpace(&lo_space_);

    DCHECK(heap->CanExpandOldGeneration(0));
    heap->NotifyOldGenerationExpansion();

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
  }

  // Iterate the string slots, as an offset from the holders we have handles to.
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish.UpdateHandles");
    for (size_t i = 0; i < string_slots_.size(); ++i) {
      HeapObject obj = *heap_object_handles[i];
      int slot_offset = string_slots_[i].slot_offset;

      // There's currently no cases where the holder object could have been
      // resized.
      DCHECK_LT(slot_offset, obj.Size());

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
          HeapObject obj = *heap_object_handles[i];
          String value = *internalized_string;
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
  return result.ToObjectChecked();
}

HeapObject OffThreadHeap::CreateFillerObjectAt(
    Address addr, int size, ClearFreedMemoryMode clear_memory_mode) {
  ReadOnlyRoots roots(this);
  HeapObject filler =
      Heap::CreateFillerObjectAt(roots, addr, size, clear_memory_mode);
  return filler;
}

}  // namespace internal
}  // namespace v8

// Undefine the heap manipulation macros.
#include "src/objects/object-macros-undef.h"
