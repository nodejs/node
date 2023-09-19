// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(CPPGC_YOUNG_GENERATION)

#include "src/heap/cppgc/remembered-set.h"

#include <algorithm>

#include "include/cppgc/member.h"
#include "include/cppgc/visitor.h"
#include "src/heap/base/basic-slot-set.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/marking-state.h"

namespace cppgc {
namespace internal {

namespace {

enum class SlotType { kCompressed, kUncompressed };

void EraseFromSet(std::set<void*>& set, void* begin, void* end) {
  // TODO(1029379): The 2 binary walks can be optimized with a custom algorithm.
  auto from = set.lower_bound(begin), to = set.lower_bound(end);
  set.erase(from, to);
}

// TODO(1029379): Make the implementation functions private functions of
// OldToNewRememberedSet to avoid parameter passing.
void InvalidateCompressedRememberedSlots(
    const HeapBase& heap, void* begin, void* end,
    std::set<void*>& remembered_slots_for_verification) {
  DCHECK_LT(begin, end);

  BasePage* page = BasePage::FromInnerAddress(&heap, begin);
  DCHECK_NOT_NULL(page);
  // The input range must reside within the same page.
  DCHECK_EQ(page, BasePage::FromInnerAddress(
                      &heap, reinterpret_cast<void*>(
                                 reinterpret_cast<uintptr_t>(end) - 1)));

  auto* slot_set = page->slot_set();
  if (!slot_set) return;

  const size_t buckets_size = SlotSet::BucketsForSize(page->AllocatedSize());

  const uintptr_t page_start = reinterpret_cast<uintptr_t>(page);
  const uintptr_t ubegin = reinterpret_cast<uintptr_t>(begin);
  const uintptr_t uend = reinterpret_cast<uintptr_t>(end);

  slot_set->RemoveRange(ubegin - page_start, uend - page_start, buckets_size,
                        SlotSet::EmptyBucketMode::FREE_EMPTY_BUCKETS);
#if DEBUG
  EraseFromSet(remembered_slots_for_verification, begin, end);
#endif  // DEBUG
}

void InvalidateUncompressedRememberedSlots(
    std::set<void*>& slots, void* begin, void* end,
    std::set<void*>& remembered_slots_for_verification) {
  EraseFromSet(slots, begin, end);
#if DEBUG
  EraseFromSet(remembered_slots_for_verification, begin, end);
#endif  // DEBUG
#if defined(ENABLE_SLOW_DCHECKS)
  // Check that no remembered slots are referring to the freed area.
  DCHECK(std::none_of(slots.begin(), slots.end(), [begin, end](void* slot) {
    void* value = nullptr;
    value = *reinterpret_cast<void**>(slot);
    return begin <= value && value < end;
  }));
#endif  // defined(ENABLE_SLOW_DCHECKS)
}

// Visit remembered set that was recorded in the generational barrier.
template <SlotType slot_type>
void VisitSlot(const HeapBase& heap, const BasePage& page, Address slot,
               MutatorMarkingState& marking_state,
               const std::set<void*>& slots_for_verification) {
#if defined(DEBUG)
  DCHECK_EQ(BasePage::FromInnerAddress(&heap, slot), &page);
  DCHECK_NE(slots_for_verification.end(), slots_for_verification.find(slot));
#endif  // defined(DEBUG)

  // Slot must always point to a valid, not freed object.
  auto& slot_header = page.ObjectHeaderFromInnerAddress(slot);
  // The age checking in the generational barrier is imprecise, since a card
  // may have mixed young/old objects. Check here precisely if the object is
  // old.
  if (slot_header.IsYoung()) return;

#if defined(CPPGC_POINTER_COMPRESSION)
  void* value = nullptr;
  if constexpr (slot_type == SlotType::kCompressed) {
    value = CompressedPointer::Decompress(*reinterpret_cast<uint32_t*>(slot));
  } else {
    value = *reinterpret_cast<void**>(slot);
  }
#else   // !defined(CPPGC_POINTER_COMPRESSION)
  void* value = *reinterpret_cast<void**>(slot);
#endif  // !defined(CPPGC_POINTER_COMPRESSION)

  // Slot could be updated to nullptr or kSentinelPointer by the mutator.
  if (value == kSentinelPointer || value == nullptr) return;

#if defined(DEBUG)
  // Check that the slot can not point to a freed object.
  HeapObjectHeader& header =
      BasePage::FromPayload(value)->ObjectHeaderFromInnerAddress(value);
  DCHECK(!header.IsFree());
#endif  // defined(DEBUG)

  marking_state.DynamicallyMarkAddress(static_cast<Address>(value));
}

class CompressedSlotVisitor : HeapVisitor<CompressedSlotVisitor> {
  friend class HeapVisitor<CompressedSlotVisitor>;

 public:
  CompressedSlotVisitor(HeapBase& heap, MutatorMarkingState& marking_state,
                        const std::set<void*>& slots_for_verification)
      : heap_(heap),
        marking_state_(marking_state),
        remembered_slots_for_verification_(slots_for_verification) {}

  size_t Run() {
    Traverse(heap_.raw_heap());
    return objects_visited_;
  }

 private:
  heap::base::SlotCallbackResult VisitCompressedSlot(Address slot) {
    DCHECK(current_page_);
    VisitSlot<SlotType::kCompressed>(heap_, *current_page_, slot,
                                     marking_state_,
                                     remembered_slots_for_verification_);
    ++objects_visited_;
    return heap::base::KEEP_SLOT;
  }

  void VisitSlotSet(SlotSet* slot_set) {
    DCHECK(current_page_);

    if (!slot_set) return;

    const uintptr_t page_start = reinterpret_cast<uintptr_t>(current_page_);
    const size_t buckets_size =
        SlotSet::BucketsForSize(current_page_->AllocatedSize());

    slot_set->Iterate(
        page_start, 0, buckets_size,
        [this](SlotSet::Address slot) {
          return VisitCompressedSlot(reinterpret_cast<Address>(slot));
        },
        SlotSet::EmptyBucketMode::FREE_EMPTY_BUCKETS);
  }

  bool VisitNormalPage(NormalPage& page) {
    current_page_ = &page;
    VisitSlotSet(page.slot_set());
    return true;
  }

  bool VisitLargePage(LargePage& page) {
    current_page_ = &page;
    VisitSlotSet(page.slot_set());
    return true;
  }

  HeapBase& heap_;
  MutatorMarkingState& marking_state_;
  BasePage* current_page_ = nullptr;

  const std::set<void*>& remembered_slots_for_verification_;
  size_t objects_visited_ = 0u;
};

class SlotRemover : HeapVisitor<SlotRemover> {
  friend class HeapVisitor<SlotRemover>;

 public:
  explicit SlotRemover(HeapBase& heap) : heap_(heap) {}

  void Run() { Traverse(heap_.raw_heap()); }

 private:
  bool VisitNormalPage(NormalPage& page) {
    page.ResetSlotSet();
    return true;
  }

  bool VisitLargePage(LargePage& page) {
    page.ResetSlotSet();
    return true;
  }

  HeapBase& heap_;
};

// Visit remembered set that was recorded in the generational barrier.
void VisitRememberedSlots(
    HeapBase& heap, MutatorMarkingState& mutator_marking_state,
    const std::set<void*>& remembered_uncompressed_slots,
    const std::set<void*>& remembered_slots_for_verification) {
  size_t objects_visited = 0;
  {
    CompressedSlotVisitor slot_visitor(heap, mutator_marking_state,
                                       remembered_slots_for_verification);
    objects_visited += slot_visitor.Run();
  }
  for (void* uncompressed_slot : remembered_uncompressed_slots) {
    auto* page = BasePage::FromInnerAddress(&heap, uncompressed_slot);
    DCHECK(page);
    VisitSlot<SlotType::kUncompressed>(
        heap, *page, static_cast<Address>(uncompressed_slot),
        mutator_marking_state, remembered_slots_for_verification);
    ++objects_visited;
  }
  DCHECK_EQ(remembered_slots_for_verification.size(), objects_visited);
  USE(objects_visited);
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

    const TraceCallback trace_callback =
        GlobalGCInfoTable::GCInfoFromIndex(source_hoh->GetGCInfoIndex()).trace;

    // Process eagerly to avoid reaccounting.
    trace_callback(&visitor, source_hoh->ObjectStart());
  }
}

// Revisit in-construction objects from previous GCs. We must do it to make
// sure that we don't miss any initializing pointer writes if a previous GC
// happened while an object was in-construction.
void RevisitInConstructionObjects(
    std::set<HeapObjectHeader*>& remembered_in_construction_objects,
    Visitor& visitor, ConservativeTracingVisitor& conservative_visitor) {
  for (HeapObjectHeader* hoh : remembered_in_construction_objects) {
    DCHECK(hoh);
    // The object must be marked on previous GC.
    DCHECK(hoh->IsMarked());

    if (hoh->template IsInConstruction<AccessMode::kNonAtomic>()) {
      conservative_visitor.TraceConservatively(*hoh);
    } else {
      // If the object is fully constructed, trace precisely.
      const TraceCallback trace_callback =
          GlobalGCInfoTable::GCInfoFromIndex(hoh->GetGCInfoIndex()).trace;
      trace_callback(&visitor, hoh->ObjectStart());
    }
  }
}

}  // namespace

void OldToNewRememberedSet::AddSlot(void* slot) {
  DCHECK(heap_.generational_gc_supported());

  BasePage* source_page = BasePage::FromInnerAddress(&heap_, slot);
  DCHECK(source_page);

  auto& slot_set = source_page->GetOrAllocateSlotSet();

  const uintptr_t slot_offset = reinterpret_cast<uintptr_t>(slot) -
                                reinterpret_cast<uintptr_t>(source_page);

  slot_set.Insert<SlotSet::AccessMode::NON_ATOMIC>(
      static_cast<size_t>(slot_offset));

#if defined(DEBUG)
  remembered_slots_for_verification_.insert(slot);
#endif  // defined(DEBUG)
}

void OldToNewRememberedSet::AddUncompressedSlot(void* uncompressed_slot) {
  DCHECK(heap_.generational_gc_supported());
  remembered_uncompressed_slots_.insert(uncompressed_slot);
#if defined(DEBUG)
  remembered_slots_for_verification_.insert(uncompressed_slot);
#endif  // defined(DEBUG)
}

void OldToNewRememberedSet::AddSourceObject(HeapObjectHeader& hoh) {
  DCHECK(heap_.generational_gc_supported());
  remembered_source_objects_.insert(&hoh);
}

void OldToNewRememberedSet::AddWeakCallback(WeakCallbackItem item) {
  DCHECK(heap_.generational_gc_supported());
  // TODO(1029379): WeakCallbacks are also executed for weak collections.
  // Consider splitting weak-callbacks in custom weak callbacks and ones for
  // collections.
  remembered_weak_callbacks_.insert(item);
}

void OldToNewRememberedSet::AddInConstructionObjectToBeRetraced(
    HeapObjectHeader& hoh) {
  DCHECK(heap_.generational_gc_supported());
  remembered_in_construction_objects_.current.insert(&hoh);
}

void OldToNewRememberedSet::InvalidateRememberedSlotsInRange(void* begin,
                                                             void* end) {
  DCHECK(heap_.generational_gc_supported());
  InvalidateCompressedRememberedSlots(heap_, begin, end,
                                      remembered_slots_for_verification_);
  InvalidateUncompressedRememberedSlots(remembered_uncompressed_slots_, begin,
                                        end,
                                        remembered_slots_for_verification_);
}

void OldToNewRememberedSet::InvalidateRememberedSourceObject(
    HeapObjectHeader& header) {
  DCHECK(heap_.generational_gc_supported());
  remembered_source_objects_.erase(&header);
}

void OldToNewRememberedSet::Visit(
    Visitor& visitor, ConservativeTracingVisitor& conservative_visitor,
    MutatorMarkingState& marking_state) {
  DCHECK(heap_.generational_gc_supported());
  VisitRememberedSlots(heap_, marking_state, remembered_uncompressed_slots_,
                       remembered_slots_for_verification_);
  VisitRememberedSourceObjects(remembered_source_objects_, visitor);
  RevisitInConstructionObjects(remembered_in_construction_objects_.previous,
                               visitor, conservative_visitor);
}

void OldToNewRememberedSet::ExecuteCustomCallbacks(LivenessBroker broker) {
  DCHECK(heap_.generational_gc_supported());
  for (const auto& callback : remembered_weak_callbacks_) {
    callback.callback(broker, callback.parameter);
  }
}

void OldToNewRememberedSet::ReleaseCustomCallbacks() {
  DCHECK(heap_.generational_gc_supported());
  remembered_weak_callbacks_.clear();
}

void OldToNewRememberedSet::Reset() {
  DCHECK(heap_.generational_gc_supported());
  SlotRemover slot_remover(heap_);
  slot_remover.Run();
  remembered_uncompressed_slots_.clear();
  remembered_source_objects_.clear();
#if DEBUG
  remembered_slots_for_verification_.clear();
#endif  // DEBUG
  remembered_in_construction_objects_.Reset();
  // Custom weak callbacks is alive across GCs.
}

bool OldToNewRememberedSet::IsEmpty() const {
  // TODO(1029379): Add visitor to check if empty.
  return remembered_uncompressed_slots_.empty() &&
         remembered_source_objects_.empty() &&
         remembered_weak_callbacks_.empty();
}

void OldToNewRememberedSet::RememberedInConstructionObjects::Reset() {
  // Make sure to keep the still-in-construction objects in the remembered set,
  // as otherwise, being marked, the marker won't be able to observe them.
  std::copy_if(previous.begin(), previous.end(),
               std::inserter(current, current.begin()),
               [](const HeapObjectHeader* h) {
                 return h->template IsInConstruction<AccessMode::kNonAtomic>();
               });
  previous = std::move(current);
  current.clear();
}

}  // namespace internal
}  // namespace cppgc

#endif  // defined(CPPGC_YOUNG_GENERATION)
