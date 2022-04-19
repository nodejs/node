// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/remembered-set.h"

#include <algorithm>

#include "include/cppgc/visitor.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/marking-state.h"

namespace cppgc {
namespace internal {

namespace {

// Visit remembered set that was recorded in the generational barrier.
void VisitRememberedSlots(const std::set<void*>& slots, const HeapBase& heap,
                          MutatorMarkingState& mutator_marking_state) {
  for (void* slot : slots) {
    // Slot must always point to a valid, not freed object.
    auto& slot_header = BasePage::FromInnerAddress(&heap, slot)
                            ->ObjectHeaderFromInnerAddress(slot);
    // The age checking in the generational barrier is imprecise, since a card
    // may have mixed young/old objects. Check here precisely if the object is
    // old.
    if (slot_header.IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(!slot_header.template IsInConstruction<AccessMode::kNonAtomic>());

    void* value = *reinterpret_cast<void**>(slot);
    // Slot could be updated to nullptr or kSentinelPointer by the mutator.
    if (value == kSentinelPointer || value == nullptr) continue;

#if DEBUG
    // Check that the slot can not point to a freed object.
    HeapObjectHeader& header =
        BasePage::FromPayload(value)->ObjectHeaderFromInnerAddress(value);
    DCHECK(!header.IsFree());
#endif

    mutator_marking_state.DynamicallyMarkAddress(static_cast<Address>(value));
  }
}

// Visits source objects that were recorded in the generational barrier for
// slots.
void VisitRememberedSourceObjects(
    const std::set<HeapObjectHeader*>& remembered_source_objects,
    Visitor& visitor) {
  for (HeapObjectHeader* source_hoh : remembered_source_objects) {
    DCHECK(source_hoh);
    // The age checking in the generational barrier is imprecise, since a card
    // may have mixed young/old objects. Check here precisely if the object is
    // old.
    if (source_hoh->IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(!source_hoh->template IsInConstruction<AccessMode::kNonAtomic>());

    const TraceCallback trace_callback =
        GlobalGCInfoTable::GCInfoFromIndex(source_hoh->GetGCInfoIndex()).trace;

    // Process eagerly to avoid reaccounting.
    trace_callback(&visitor, source_hoh->ObjectStart());
  }
}

}  // namespace

void OldToNewRememberedSet::AddSlot(void* slot) {
  remembered_slots_.insert(slot);
}

void OldToNewRememberedSet::AddSourceObject(HeapObjectHeader& hoh) {
  remembered_source_objects_.insert(&hoh);
}

void OldToNewRememberedSet::AddWeakCallback(WeakCallbackItem item) {
  // TODO(1029379): WeakCallbacks are also executed for weak collections.
  // Consider splitting weak-callbacks in custom weak callbacks and ones for
  // collections.
  remembered_weak_callbacks_.insert(item);
}

void OldToNewRememberedSet::InvalidateRememberedSlotsInRange(void* begin,
                                                             void* end) {
  // TODO(1029379): The 2 binary walks can be optimized with a custom algorithm.
  auto from = remembered_slots_.lower_bound(begin),
       to = remembered_slots_.lower_bound(end);
  remembered_slots_.erase(from, to);
#if defined(ENABLE_SLOW_DCHECKS)
  // Check that no remembered slots are referring to the freed area.
  DCHECK(std::none_of(remembered_slots_.begin(), remembered_slots_.end(),
                      [begin, end](void* slot) {
                        void* value = *reinterpret_cast<void**>(slot);
                        return begin <= value && value < end;
                      }));
#endif  // defined(ENABLE_SLOW_DCHECKS)
}

void OldToNewRememberedSet::InvalidateRememberedSourceObject(
    HeapObjectHeader& header) {
  remembered_source_objects_.erase(&header);
}

void OldToNewRememberedSet::Visit(Visitor& visitor,
                                  MutatorMarkingState& marking_state) {
  VisitRememberedSlots(remembered_slots_, heap_, marking_state);
  VisitRememberedSourceObjects(remembered_source_objects_, visitor);
}

void OldToNewRememberedSet::ExecuteCustomCallbacks(LivenessBroker broker) {
  for (const auto& callback : remembered_weak_callbacks_) {
    callback.callback(broker, callback.parameter);
  }
}

void OldToNewRememberedSet::ReleaseCustomCallbacks() {
  remembered_weak_callbacks_.clear();
}

void OldToNewRememberedSet::Reset() {
  remembered_slots_.clear();
  remembered_source_objects_.clear();
}

}  // namespace internal
}  // namespace cppgc
