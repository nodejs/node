// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenger.h"

#include "src/heap/barrier.h"
#include "src/heap/heap-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/sweeper.h"
#include "src/objects-body-descriptors-inl.h"

namespace v8 {
namespace internal {

class IterateAndScavengePromotedObjectsVisitor final : public ObjectVisitor {
 public:
  IterateAndScavengePromotedObjectsVisitor(Heap* heap, Scavenger* scavenger,
                                           bool record_slots)
      : heap_(heap), scavenger_(scavenger), record_slots_(record_slots) {}

  inline void VisitPointers(HeapObject* host, Object** start,
                            Object** end) final {
    for (Object** slot = start; slot < end; ++slot) {
      Object* target = *slot;
      DCHECK(!HasWeakHeapObjectTag(target));
      if (target->IsHeapObject()) {
        HandleSlot(host, reinterpret_cast<Address>(slot),
                   HeapObject::cast(target));
      }
    }
  }

  inline void VisitPointers(HeapObject* host, MaybeObject** start,
                            MaybeObject** end) final {
    // Treat weak references as strong. TODO(marja): Proper weakness handling in
    // the young generation.
    for (MaybeObject** slot = start; slot < end; ++slot) {
      MaybeObject* target = *slot;
      HeapObject* heap_object;
      if (target->ToStrongOrWeakHeapObject(&heap_object)) {
        HandleSlot(host, reinterpret_cast<Address>(slot), heap_object);
      }
    }
  }

  inline void HandleSlot(HeapObject* host, Address slot_address,
                         HeapObject* target) {
    HeapObjectReference** slot =
        reinterpret_cast<HeapObjectReference**>(slot_address);
    scavenger_->PageMemoryFence(reinterpret_cast<MaybeObject*>(target));

    if (heap_->InFromSpace(target)) {
      scavenger_->ScavengeObject(slot, target);
      bool success = (*slot)->ToStrongOrWeakHeapObject(&target);
      USE(success);
      DCHECK(success);
      scavenger_->PageMemoryFence(reinterpret_cast<MaybeObject*>(target));

      if (heap_->InNewSpace(target)) {
        SLOW_DCHECK(target->IsHeapObject());
        SLOW_DCHECK(heap_->InToSpace(target));
        RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot_address),
                                          slot_address);
      }
      SLOW_DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(
          HeapObject::cast(target)));
    } else if (record_slots_ && MarkCompactCollector::IsOnEvacuationCandidate(
                                    HeapObject::cast(target))) {
      heap_->mark_compact_collector()->RecordSlot(host, slot, target);
    }
  }

 private:
  Heap* const heap_;
  Scavenger* const scavenger_;
  const bool record_slots_;
};

Scavenger::Scavenger(Heap* heap, bool is_logging, CopiedList* copied_list,
                     PromotionList* promotion_list, int task_id)
    : heap_(heap),
      promotion_list_(promotion_list, task_id),
      copied_list_(copied_list, task_id),
      local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
      copied_size_(0),
      promoted_size_(0),
      allocator_(heap),
      is_logging_(is_logging),
      is_incremental_marking_(heap->incremental_marking()->IsMarking()),
      is_compacting_(heap->incremental_marking()->IsCompacting()) {}

void Scavenger::IterateAndScavengePromotedObject(HeapObject* target, int size) {
  // We are not collecting slots on new space objects during mutation thus we
  // have to scan for pointers to evacuation candidates when we promote
  // objects. But we should not record any slots in non-black objects. Grey
  // object's slots would be rescanned. White object might not survive until
  // the end of collection it would be a violation of the invariant to record
  // its slots.
  const bool record_slots =
      is_compacting_ &&
      heap()->incremental_marking()->atomic_marking_state()->IsBlack(target);
  IterateAndScavengePromotedObjectsVisitor visitor(heap(), this, record_slots);
  target->IterateBodyFast(target->map(), size, &visitor);
}

void Scavenger::AddPageToSweeperIfNecessary(MemoryChunk* page) {
  AllocationSpace space = page->owner()->identity();
  if ((space == OLD_SPACE) && !page->SweepingDone()) {
    heap()->mark_compact_collector()->sweeper()->AddPage(
        space, reinterpret_cast<Page*>(page),
        Sweeper::READD_TEMPORARY_REMOVED_PAGE);
  }
}

void Scavenger::ScavengePage(MemoryChunk* page) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "Scavenger::ScavengePage");
  CodePageMemoryModificationScope memory_modification_scope(page);
  RememberedSet<OLD_TO_NEW>::Iterate(
      page,
      [this](Address addr) { return CheckAndScavengeObject(heap_, addr); },
      SlotSet::KEEP_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_NEW>::IterateTyped(
      page, [this](SlotType type, Address host_addr, Address addr) {
        return UpdateTypedSlotHelper::UpdateTypedSlot(
            heap_->isolate(), type, addr, [this](MaybeObject** addr) {
              return CheckAndScavengeObject(heap(),
                                            reinterpret_cast<Address>(addr));
            });
      });

  AddPageToSweeperIfNecessary(page);
}

void Scavenger::Process(OneshotBarrier* barrier) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "Scavenger::Process");
  // Threshold when to switch processing the promotion list to avoid
  // allocating too much backing store in the worklist.
  const int kProcessPromotionListThreshold = kPromotionListSegmentSize / 2;
  ScavengeVisitor scavenge_visitor(heap(), this);

  const bool have_barrier = barrier != nullptr;
  bool done;
  size_t objects = 0;
  do {
    done = true;
    ObjectAndSize object_and_size;
    while ((promotion_list_.LocalPushSegmentSize() <
            kProcessPromotionListThreshold) &&
           copied_list_.Pop(&object_and_size)) {
      scavenge_visitor.Visit(object_and_size.first);
      done = false;
      if (have_barrier && ((++objects % kInterruptThreshold) == 0)) {
        if (!copied_list_.IsGlobalPoolEmpty()) {
          barrier->NotifyAll();
        }
      }
    }

    while (promotion_list_.Pop(&object_and_size)) {
      HeapObject* target = object_and_size.first;
      int size = object_and_size.second;
      DCHECK(!target->IsMap());
      IterateAndScavengePromotedObject(target, size);
      done = false;
      if (have_barrier && ((++objects % kInterruptThreshold) == 0)) {
        if (!promotion_list_.IsGlobalPoolEmpty()) {
          barrier->NotifyAll();
        }
      }
    }
  } while (!done);
}

void Scavenger::Finalize() {
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
  heap()->IncrementSemiSpaceCopiedObjectSize(copied_size_);
  heap()->IncrementPromotedObjectsSize(promoted_size_);
  allocator_.Finalize();
}

void RootScavengeVisitor::VisitRootPointer(Root root, const char* description,
                                           Object** p) {
  DCHECK(!HasWeakHeapObjectTag(*p));
  ScavengePointer(p);
}

void RootScavengeVisitor::VisitRootPointers(Root root, const char* description,
                                            Object** start, Object** end) {
  // Copy all HeapObject pointers in [start, end)
  for (Object** p = start; p < end; p++) ScavengePointer(p);
}

void RootScavengeVisitor::ScavengePointer(Object** p) {
  Object* object = *p;
  DCHECK(!HasWeakHeapObjectTag(object));
  if (!heap_->InNewSpace(object)) return;

  scavenger_->ScavengeObject(reinterpret_cast<HeapObjectReference**>(p),
                             reinterpret_cast<HeapObject*>(object));
}

}  // namespace internal
}  // namespace v8
