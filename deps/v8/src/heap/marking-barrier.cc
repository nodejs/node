// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking-barrier.h"

#include "src/base/logging.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-barrier-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/safepoint.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer.h"

namespace v8 {
namespace internal {

MarkingBarrier::MarkingBarrier(LocalHeap* local_heap)
    : heap_(local_heap->heap()),
      major_collector_(heap_->mark_compact_collector()),
      minor_collector_(heap_->minor_mark_compact_collector()),
      incremental_marking_(heap_->incremental_marking()),
      major_worklist_(*major_collector_->marking_worklists()->shared()),
      minor_worklist_(*minor_collector_->marking_worklists()->shared()),
      marking_state_(heap_->isolate()),
      is_main_thread_barrier_(local_heap->is_main_thread()),
      uses_shared_heap_(heap_->isolate()->has_shared_heap()),
      is_shared_heap_isolate_(heap_->isolate()->is_shared_heap_isolate()) {}

MarkingBarrier::~MarkingBarrier() { DCHECK(typed_slots_map_.empty()); }

void MarkingBarrier::Write(HeapObject host, HeapObjectSlot slot,
                           HeapObject value) {
  DCHECK(IsCurrentMarkingBarrier());
  if (MarkValue(host, value)) {
    if (is_compacting_ && slot.address()) {
      DCHECK(is_major());
      major_collector_->RecordSlot(host, slot, value);
    }
  }
}

void MarkingBarrier::WriteWithoutHost(HeapObject value) {
  DCHECK(is_main_thread_barrier_);
  if (is_minor() && !Heap::InYoungGeneration(value)) return;

  if (WhiteToGreyAndPush(value)) {
    if (V8_UNLIKELY(v8_flags.track_retaining_path) && is_major()) {
      heap_->AddRetainingRoot(Root::kWriteBarrier, value);
    }
  }
}

void MarkingBarrier::Write(Code host, RelocInfo* reloc_info, HeapObject value) {
  DCHECK(IsCurrentMarkingBarrier());
  if (MarkValue(host, value)) {
    if (is_compacting_) {
      DCHECK(is_major());
      if (is_main_thread_barrier_) {
        // An optimization to avoid allocating additional typed slots for the
        // main thread.
        major_collector_->RecordRelocSlot(host, reloc_info, value);
      } else {
        RecordRelocSlot(host, reloc_info, value);
      }
    }
  }
}

void MarkingBarrier::Write(JSArrayBuffer host,
                           ArrayBufferExtension* extension) {
  DCHECK(IsCurrentMarkingBarrier());
  if (!V8_CONCURRENT_MARKING_BOOL && !marking_state_.IsBlack(host)) {
    // The extension will be marked when the marker visits the host object.
    return;
  }
  if (is_minor()) {
    if (Heap::InYoungGeneration(host)) {
      extension->YoungMark();
    }
  } else {
    extension->Mark();
  }
}

void MarkingBarrier::Write(DescriptorArray descriptor_array,
                           int number_of_own_descriptors) {
  DCHECK(IsCurrentMarkingBarrier());
  DCHECK(IsReadOnlyHeapObject(descriptor_array.map()));

  if (is_minor() && !heap_->InYoungGeneration(descriptor_array)) return;

  // The DescriptorArray needs to be marked black here to ensure that slots are
  // recorded by the Scavenger in case the DescriptorArray is promoted while
  // incremental marking is running. This is needed as the regular marking
  // visitor does not re-process any already marked descriptors. If we don't
  // mark it black here, the Scavenger may promote a DescriptorArray and any
  // already marked descriptors will not have any slots recorded.
  if (!marking_state_.IsBlack(descriptor_array)) {
    marking_state_.WhiteToGrey(descriptor_array);
    marking_state_.GreyToBlack(descriptor_array);
    MarkRange(descriptor_array, descriptor_array.GetFirstPointerSlot(),
              descriptor_array.GetDescriptorSlot(0));
  }

  // Concurrent MinorMC always marks the full young generation DescriptorArray.
  // We cannot use epoch like MajorMC does because only the lower 2 bits are
  // used, and with many MinorMC cycles this could lead to correctness issues.
  const int16_t old_marked =
      is_minor() ? 0
                 : descriptor_array.UpdateNumberOfMarkedDescriptors(
                       major_collector_->epoch(), number_of_own_descriptors);
  if (old_marked < number_of_own_descriptors) {
    // This marks the range from [old_marked, number_of_own_descriptors) instead
    // of registering weak slots which may temporarily hold alive more objects
    // for the current GC cycle. Weakness is not needed for actual trimming, see
    // `MarkCompactCollector::TrimDescriptorArray()`.
    MarkRange(descriptor_array,
              MaybeObjectSlot(descriptor_array.GetDescriptorSlot(old_marked)),
              MaybeObjectSlot(descriptor_array.GetDescriptorSlot(
                  number_of_own_descriptors)));
  }
}

void MarkingBarrier::RecordRelocSlot(Code host, RelocInfo* rinfo,
                                     HeapObject target) {
  DCHECK(IsCurrentMarkingBarrier());
  if (!MarkCompactCollector::ShouldRecordRelocSlot(host, rinfo, target)) return;

  MarkCompactCollector::RecordRelocSlotInfo info =
      MarkCompactCollector::ProcessRelocInfo(host, rinfo, target);

  auto& typed_slots = typed_slots_map_[info.memory_chunk];
  if (!typed_slots) {
    typed_slots.reset(new TypedSlots());
  }
  typed_slots->Insert(info.slot_type, info.offset);
}

// static
void MarkingBarrier::ActivateAll(Heap* heap, bool is_compacting,
                                 MarkingBarrierType marking_barrier_type) {
  heap->safepoint()->IterateLocalHeaps(
      [is_compacting, marking_barrier_type](LocalHeap* local_heap) {
        local_heap->marking_barrier()->Activate(is_compacting,
                                                marking_barrier_type);
      });
}

// static
void MarkingBarrier::DeactivateAll(Heap* heap) {
  heap->safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->marking_barrier()->Deactivate();
  });
}

// static
void MarkingBarrier::PublishAll(Heap* heap) {
  heap->safepoint()->IterateLocalHeaps(
      [](LocalHeap* local_heap) { local_heap->marking_barrier()->Publish(); });
}

void MarkingBarrier::Publish() {
  if (is_activated_) {
    current_worklist_->Publish();
    base::Optional<CodePageHeaderModificationScope> optional_rwx_write_scope;
    if (!typed_slots_map_.empty()) {
      optional_rwx_write_scope.emplace(
          "Merging typed slots may require allocating a new typed slot set.");
    }
    for (auto& it : typed_slots_map_) {
      MemoryChunk* memory_chunk = it.first;
      // Access to TypeSlots need to be protected, since LocalHeaps might
      // publish code in the background thread.
      base::Optional<base::MutexGuard> opt_guard;
      if (v8_flags.concurrent_sparkplug) {
        opt_guard.emplace(memory_chunk->mutex());
      }
      std::unique_ptr<TypedSlots>& typed_slots = it.second;
      RememberedSet<OLD_TO_OLD>::MergeTyped(memory_chunk,
                                            std::move(typed_slots));
    }
    typed_slots_map_.clear();
  }
}

void MarkingBarrier::DeactivateSpace(PagedSpace* space) {
  DCHECK(is_main_thread_barrier_);
  for (Page* p : *space) {
    p->SetOldGenerationPageFlags(false);
  }
}

void MarkingBarrier::DeactivateSpace(NewSpace* space) {
  DCHECK(is_main_thread_barrier_);
  for (Page* p : *space) {
    p->SetYoungGenerationPageFlags(false);
  }
}

void MarkingBarrier::Deactivate() {
  is_activated_ = false;
  is_compacting_ = false;
  if (is_main_thread_barrier_) {
    DeactivateSpace(heap_->old_space());
    if (heap_->map_space()) DeactivateSpace(heap_->map_space());
    DeactivateSpace(heap_->code_space());
    DeactivateSpace(heap_->new_space());
    if (heap_->shared_space()) {
      DeactivateSpace(heap_->shared_space());
    }
    for (LargePage* p : *heap_->new_lo_space()) {
      p->SetYoungGenerationPageFlags(false);
      DCHECK(p->IsLargePage());
    }
    for (LargePage* p : *heap_->lo_space()) {
      p->SetOldGenerationPageFlags(false);
    }
    for (LargePage* p : *heap_->code_lo_space()) {
      p->SetOldGenerationPageFlags(false);
    }
    if (heap_->shared_lo_space()) {
      for (LargePage* p : *heap_->shared_lo_space()) {
        p->SetOldGenerationPageFlags(false);
      }
    }
  }
  DCHECK(typed_slots_map_.empty());
  DCHECK(current_worklist_->IsLocalEmpty());
}

void MarkingBarrier::ActivateSpace(PagedSpace* space) {
  DCHECK(is_main_thread_barrier_);
  for (Page* p : *space) {
    p->SetOldGenerationPageFlags(true);
  }
}

void MarkingBarrier::ActivateSpace(NewSpace* space) {
  DCHECK(is_main_thread_barrier_);
  for (Page* p : *space) {
    p->SetYoungGenerationPageFlags(true);
  }
}

void MarkingBarrier::Activate(bool is_compacting,
                              MarkingBarrierType marking_barrier_type) {
  DCHECK(!is_activated_);
  DCHECK(major_worklist_.IsLocalEmpty());
  DCHECK(minor_worklist_.IsLocalEmpty());
  is_compacting_ = is_compacting;
  marking_barrier_type_ = marking_barrier_type;
  current_worklist_ = is_minor() ? &minor_worklist_ : &major_worklist_;
  is_activated_ = true;
  if (is_main_thread_barrier_) {
    ActivateSpace(heap_->old_space());
    if (heap_->map_space()) ActivateSpace(heap_->map_space());
    {
      CodePageHeaderModificationScope rwx_write_scope(
          "Modification of Code page header flags requires write access");
      ActivateSpace(heap_->code_space());
    }
    ActivateSpace(heap_->new_space());
    if (heap_->shared_space()) {
      ActivateSpace(heap_->shared_space());
    }

    for (LargePage* p : *heap_->new_lo_space()) {
      p->SetYoungGenerationPageFlags(true);
      DCHECK(p->IsLargePage());
    }

    for (LargePage* p : *heap_->lo_space()) {
      p->SetOldGenerationPageFlags(true);
    }

    {
      CodePageHeaderModificationScope rwx_write_scope(
          "Modification of Code page header flags requires write access");
      for (LargePage* p : *heap_->code_lo_space()) {
        p->SetOldGenerationPageFlags(true);
      }
    }

    if (heap_->shared_lo_space()) {
      for (LargePage* p : *heap_->shared_lo_space()) {
        p->SetOldGenerationPageFlags(true);
      }
    }
  }
}

bool MarkingBarrier::IsCurrentMarkingBarrier() {
  return WriteBarrier::CurrentMarkingBarrier(heap_) == this;
}

}  // namespace internal
}  // namespace v8
