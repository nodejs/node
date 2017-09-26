// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenger.h"

#include "src/heap/heap-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/scavenger-inl.h"
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
    for (Address slot_address = reinterpret_cast<Address>(start);
         slot_address < reinterpret_cast<Address>(end);
         slot_address += kPointerSize) {
      Object** slot = reinterpret_cast<Object**>(slot_address);
      Object* target = *slot;

      if (target->IsHeapObject()) {
        if (heap_->InFromSpace(target)) {
          scavenger_->ScavengeObject(reinterpret_cast<HeapObject**>(slot),
                                     HeapObject::cast(target));
          target = *slot;
          if (heap_->InNewSpace(target)) {
            SLOW_DCHECK(target->IsHeapObject());
            SLOW_DCHECK(heap_->InToSpace(target));
            RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot_address),
                                              slot_address);
          }
          SLOW_DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(
              HeapObject::cast(target)));
        } else if (record_slots_ &&
                   MarkCompactCollector::IsOnEvacuationCandidate(
                       HeapObject::cast(target))) {
          heap_->mark_compact_collector()->RecordSlot(host, slot, target);
        }
      }
    }
  }

  inline void VisitCodeEntry(JSFunction* host, Address code_entry_slot) final {
    // Black allocation is not enabled during Scavenges.
    DCHECK(!heap_->incremental_marking()->black_allocation());

    if (ObjectMarking::IsBlack(host, MarkingState::Internal(host))) {
      Code* code = Code::cast(Code::GetObjectFromEntryAddress(code_entry_slot));
      heap_->mark_compact_collector()->RecordCodeEntrySlot(
          host, code_entry_slot, code);
    }
  }

 private:
  Heap* const heap_;
  Scavenger* const scavenger_;
  const bool record_slots_;
};

void Scavenger::IterateAndScavengePromotedObject(HeapObject* target, int size) {
  // We are not collecting slots on new space objects during mutation
  // thus we have to scan for pointers to evacuation candidates when we
  // promote objects. But we should not record any slots in non-black
  // objects. Grey object's slots would be rescanned.
  // White object might not survive until the end of collection
  // it would be a violation of the invariant to record it's slots.
  const bool record_slots =
      heap()->incremental_marking()->IsCompacting() &&
      ObjectMarking::IsBlack(target, MarkingState::Internal(target));
  IterateAndScavengePromotedObjectsVisitor visitor(heap(), this, record_slots);
  if (target->IsJSFunction()) {
    // JSFunctions reachable through kNextFunctionLinkOffset are weak. Slots for
    // this links are recorded during processing of weak lists.
    JSFunction::BodyDescriptorWeak::IterateBody(target, size, &visitor);
  } else {
    target->IterateBody(target->map()->instance_type(), size, &visitor);
  }
}

void Scavenger::Process() {
  // Threshold when to switch processing the promotion list to avoid
  // allocating too much backing store in the worklist.
  const int kProcessPromotionListThreshold = kPromotionListSegmentSize / 2;
  ScavengeVisitor scavenge_visitor(heap(), this);

  bool done;
  do {
    done = true;
    AddressRange range;
    while ((promotion_list_.LocalPushSegmentSize() <
            kProcessPromotionListThreshold) &&
           copied_list_.Pop(&range)) {
      for (Address current = range.first; current < range.second;) {
        HeapObject* object = HeapObject::FromAddress(current);
        int size = object->Size();
        scavenge_visitor.Visit(object);
        current += size;
      }
      done = false;
    }
    ObjectAndSize object_and_size;
    while (promotion_list_.Pop(&object_and_size)) {
      HeapObject* target = object_and_size.first;
      int size = object_and_size.second;
      DCHECK(!target->IsMap());
      IterateAndScavengePromotedObject(target, size);
      done = false;
    }
  } while (!done);
}

void Scavenger::RecordCopiedObject(HeapObject* obj) {
  bool should_record = FLAG_log_gc;
#ifdef DEBUG
  should_record = FLAG_heap_stats;
#endif
  if (should_record) {
    if (heap()->new_space()->Contains(obj)) {
      heap()->new_space()->RecordAllocation(obj);
    } else {
      heap()->new_space()->RecordPromotion(obj);
    }
  }
}

void Scavenger::Finalize() {
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
  heap()->IncrementSemiSpaceCopiedObjectSize(copied_size_);
  heap()->IncrementPromotedObjectsSize(promoted_size_);
  allocator_.Finalize();
}

void RootScavengeVisitor::VisitRootPointer(Root root, Object** p) {
  ScavengePointer(p);
}

void RootScavengeVisitor::VisitRootPointers(Root root, Object** start,
                                            Object** end) {
  // Copy all HeapObject pointers in [start, end)
  for (Object** p = start; p < end; p++) ScavengePointer(p);
}

void RootScavengeVisitor::ScavengePointer(Object** p) {
  Object* object = *p;
  if (!heap_->InNewSpace(object)) return;

  scavenger_->ScavengeObject(reinterpret_cast<HeapObject**>(p),
                             reinterpret_cast<HeapObject*>(object));
}

}  // namespace internal
}  // namespace v8
