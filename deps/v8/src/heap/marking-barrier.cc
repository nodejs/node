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
#include "src/objects/js-array-buffer.h"

namespace v8 {
namespace internal {

MarkingBarrier::MarkingBarrier(Heap* heap, MarkCompactCollector* collector,
                               IncrementalMarking* incremental_marking)
    : heap_(heap),
      collector_(collector),
      incremental_marking_(incremental_marking) {}

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
      collector_->RecordRelocSlot(host, reloc_info, value);
    }
  }
}

void MarkingBarrier::Write(JSArrayBuffer host,
                           ArrayBufferExtension* extension) {
  if (!V8_CONCURRENT_MARKING_BOOL && marking_state_.IsBlack(host)) {
    // The extension will be marked when the marker visits the host object.
    return;
  }
  extension->Mark();
}

void MarkingBarrier::Write(Map host, DescriptorArray descriptor_array,
                           int number_of_own_descriptors) {
  int16_t raw_marked = descriptor_array.raw_number_of_marked_descriptors();
  if (NumberOfMarkedDescriptors::decode(collector_->epoch(), raw_marked) <
      number_of_own_descriptors) {
    collector_->MarkDescriptorArrayFromWriteBarrier(host, descriptor_array,
                                                    number_of_own_descriptors);
  }
}

void MarkingBarrier::Deactivate(PagedSpace* space) {
  for (Page* p : *space) {
    p->SetOldGenerationPageFlags(false);
  }
}

void MarkingBarrier::Deactivate(NewSpace* space) {
  for (Page* p : *space) {
    p->SetYoungGenerationPageFlags(false);
  }
}

void MarkingBarrier::Deactivate() {
  Deactivate(heap_->old_space());
  Deactivate(heap_->map_space());
  Deactivate(heap_->code_space());
  Deactivate(heap_->new_space());
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
  is_activated_ = false;
  is_compacting_ = false;
}

void MarkingBarrier::Activate(PagedSpace* space) {
  for (Page* p : *space) {
    p->SetOldGenerationPageFlags(true);
  }
}

void MarkingBarrier::Activate(NewSpace* space) {
  for (Page* p : *space) {
    p->SetYoungGenerationPageFlags(true);
  }
}

void MarkingBarrier::Activate(bool is_compacting) {
  DCHECK(!is_activated_);
  is_compacting_ = is_compacting;
  is_activated_ = true;
  Activate(heap_->old_space());
  Activate(heap_->map_space());
  Activate(heap_->code_space());
  Activate(heap_->new_space());

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

}  // namespace internal
}  // namespace v8
