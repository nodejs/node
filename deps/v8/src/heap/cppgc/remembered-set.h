// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_REMEMBERED_SET_H_
#define V8_HEAP_CPPGC_REMEMBERED_SET_H_

#if defined(CPPGC_YOUNG_GENERATION)

#include <set>

#include "src/base/macros.h"
#include "src/heap/base/basic-slot-set.h"
#include "src/heap/cppgc/marking-worklists.h"

namespace cppgc {

class Visitor;
class LivenessBroker;

namespace internal {

class HeapBase;
class HeapObjectHeader;
class MutatorMarkingState;

class SlotSet : public ::heap::base::BasicSlotSet<kSlotSize> {};

// OldToNewRememberedSet represents a per-heap set of old-to-new references.
class V8_EXPORT_PRIVATE OldToNewRememberedSet final {
 public:
  using WeakCallbackItem = MarkingWorklists::WeakCallbackItem;

  explicit OldToNewRememberedSet(HeapBase& heap)
      : heap_(heap), remembered_weak_callbacks_(compare_parameter) {}

  OldToNewRememberedSet(const OldToNewRememberedSet&) = delete;
  OldToNewRememberedSet& operator=(const OldToNewRememberedSet&) = delete;

  void AddSlot(void* slot);
  void AddUncompressedSlot(void* slot);
  void AddSourceObject(HeapObjectHeader& source_hoh);
  void AddWeakCallback(WeakCallbackItem);

  // Remembers an in-construction object to be retraced on the next minor GC.
  void AddInConstructionObjectToBeRetraced(HeapObjectHeader&);

  void InvalidateRememberedSlotsInRange(void* begin, void* end);
  void InvalidateRememberedSourceObject(HeapObjectHeader& source_hoh);

  void Visit(Visitor&, ConservativeTracingVisitor&, MutatorMarkingState&);

  void ExecuteCustomCallbacks(LivenessBroker);
  void ReleaseCustomCallbacks();

  void Reset();

  bool IsEmpty() const;

 private:
  friend class MinorGCTest;

  // The class keeps track of inconstruction objects that should be revisited.
  struct RememberedInConstructionObjects final {
    void Reset();

    std::set<HeapObjectHeader*> previous;
    std::set<HeapObjectHeader*> current;
  };

  static constexpr struct {
    bool operator()(const WeakCallbackItem& lhs,
                    const WeakCallbackItem& rhs) const {
      return lhs.parameter < rhs.parameter;
    }
  } compare_parameter{};

  HeapBase& heap_;
  std::set<HeapObjectHeader*> remembered_source_objects_;
  std::set<WeakCallbackItem, decltype(compare_parameter)>
      remembered_weak_callbacks_;
  // Compressed slots are stored in slot-sets (per-page two-level bitmaps),
  // whereas uncompressed are stored in std::set.
  std::set<void*> remembered_uncompressed_slots_;
  std::set<void*> remembered_slots_for_verification_;
  RememberedInConstructionObjects remembered_in_construction_objects_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // defined(CPPGC_YOUNG_GENERATION)

#endif  // V8_HEAP_CPPGC_REMEMBERED_SET_H_
