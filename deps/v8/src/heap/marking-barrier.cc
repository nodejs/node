// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking-barrier.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-barrier-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/safepoint.h"
#include "src/objects/js-array-buffer.h"

namespace v8 {
namespace internal {

MarkingBarrier::MarkingBarrier(Heap* heap)
    : heap_(heap),
      collector_(heap_->mark_compact_collector()),
      incremental_marking_(heap_->incremental_marking()),
      worklist_(collector_->marking_worklists()->shared()),
      is_main_thread_barrier_(true) {}

MarkingBarrier::MarkingBarrier(LocalHeap* local_heap)
    : heap_(local_heap->heap()),
      collector_(heap_->mark_compact_collector()),
      incremental_marking_(nullptr),
      worklist_(collector_->marking_worklists()->shared()),
      is_main_thread_barrier_(false) {}

MarkingBarrier::~MarkingBarrier() { DCHECK(worklist_.IsLocalEmpty()); }

void MarkingBarrier::Write(HeapObject host, HeapObjectSlot slot,
                           HeapObject value) {
  if (MarkValue(host, value)) {
    if (is_compacting_ && slot.address()) {
      collector_->RecordSlot(host, slot, value);
    }
  }
}

void MarkingBarrier::Write(Code host, RelocInfo* reloc_info, HeapObject value) {
  if (MarkValue(host, value)) {
    if (is_compacting_) {
      if (is_main_thread_barrier_) {
        // An optimization to avoid allocating additional typed slots for the
        // main thread.
        collector_->RecordRelocSlot(host, reloc_info, value);
      } else {
        RecordRelocSlot(host, reloc_info, value);
      }
    }
  }
}

void MarkingBarrier::Write(JSArrayBuffer host,
                           ArrayBufferExtension* extension) {
  if (!V8_CONCURRENT_MARKING_BOOL && !marking_state_.IsBlack(host)) {
    // The extension will be marked when the marker visits the host object.
    return;
  }
  extension->Mark();
}

void MarkingBarrier::Write(DescriptorArray descriptor_array,
                           int number_of_own_descriptors) {
  DCHECK(is_main_thread_barrier_);
  int16_t raw_marked = descriptor_array.raw_number_of_marked_descriptors();
  if (NumberOfMarkedDescriptors::decode(collector_->epoch(), raw_marked) <
      number_of_own_descriptors) {
    collector_->MarkDescriptorArrayFromWriteBarrier(descriptor_array,
                                                    number_of_own_descriptors);
  }
}

void MarkingBarrier::RecordRelocSlot(Code host, RelocInfo* rinfo,
                                     HeapObject target) {
  MarkCompactCollector::RecordRelocSlotInfo info =
      MarkCompactCollector::PrepareRecordRelocSlot(host, rinfo, target);
  if (info.should_record) {
    auto& typed_slots = typed_slots_map_[info.memory_chunk];
    if (!typed_slots) {
      typed_slots.reset(new TypedSlots());
    }
    typed_slots->Insert(info.slot_type, info.offset);
  }
}

// static
void MarkingBarrier::ActivateAll(Heap* heap, bool is_compacting) {
  heap->marking_barrier()->Activate(is_compacting);
  heap->safepoint()->IterateLocalHeaps([is_compacting](LocalHeap* local_heap) {
    local_heap->marking_barrier()->Activate(is_compacting);
  });
}

// static
void MarkingBarrier::DeactivateAll(Heap* heap) {
  heap->marking_barrier()->Deactivate();
  heap->safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->marking_barrier()->Deactivate();
  });
}

// static
void MarkingBarrier::PublishAll(Heap* heap) {
  heap->marking_barrier()->Publish();
  heap->safepoint()->IterateLocalHeaps(
      [](LocalHeap* local_heap) { local_heap->marking_barrier()->Publish(); });
}

void MarkingBarrier::Publish() {
  if (is_activated_) {
    worklist_.Publish();
    for (auto& it : typed_slots_map_) {
      MemoryChunk* memory_chunk = it.first;
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
    DeactivateSpace(heap_->map_space());
    DeactivateSpace(heap_->code_space());
    DeactivateSpace(heap_->new_space());
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
  }
  DCHECK(typed_slots_map_.empty());
  DCHECK(worklist_.IsLocalEmpty());
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

void MarkingBarrier::Activate(bool is_compacting) {
  DCHECK(!is_activated_);
  DCHECK(worklist_.IsLocalEmpty());
  is_compacting_ = is_compacting;
  is_activated_ = true;
  if (is_main_thread_barrier_) {
    ActivateSpace(heap_->old_space());
    ActivateSpace(heap_->map_space());
    ActivateSpace(heap_->code_space());
    ActivateSpace(heap_->new_space());

    for (LargePage* p : *heap_->new_lo_space()) {
      p->SetYoungGenerationPageFlags(true);
      DCHECK(p->IsLargePage());
    }

    for (LargePage* p : *heap_->lo_space()) {
      p->SetOldGenerationPageFlags(true);
    }

    for (LargePage* p : *heap_->code_lo_space()) {
      p->SetOldGenerationPageFlags(true);
    }
  }
}

}  // namespace internal
}  // namespace v8
