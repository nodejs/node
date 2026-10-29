// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NORMAL_PAGE_H_
#define V8_HEAP_NORMAL_PAGE_H_

#include "src/heap/base-space.h"
#include "src/heap/free-list.h"
#include "src/heap/mutable-page.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

class Heap;

// A mutable page of `kRegularPageSize` bytes.
class NormalPage final : public MutablePage {
 public:
  // Page size in bytes. This must be a multiple of the OS page size.
  static constexpr int kPageSize = kRegularPageSize;

  // Returns the page containing a given address. The address ranges
  // from [page_addr .. page_addr + kPageSize]. This only works if the object is
  // in fact in a page.
  V8_INLINE static NormalPage* FromAddress(Address addr);
  V8_INLINE static NormalPage* FromAddress(const Isolate* isolate,
                                           Address addr);
  V8_INLINE static NormalPage* FromHeapObject(Tagged<HeapObject> o);

  // Returns the page containing the address provided. The address can
  // potentially point righter after the page. To be also safe for tagged values
  // we subtract a hole word. The valid address ranges from
  // [page_addr + area_start_ .. page_addr + kPageSize + kTaggedSize].
  V8_INLINE static NormalPage* FromAllocationAreaAddress(Address address);

  // Checks if address1 and address2 are on the same new space page.
  static bool OnSamePage(Address address1, Address address2) {
    return MemoryChunk::FromAddress(address1) ==
           MemoryChunk::FromAddress(address2);
  }

  // Checks whether an address is page aligned.
  static bool IsAlignedToPageSize(Address addr) {
    return MemoryChunk::IsAligned(addr);
  }

  static NormalPage* ConvertNewToOld(NormalPage* old_page, FreeMode free_mode);

  NormalPage(Heap* heap, BaseSpace* space, size_t size, Address area_start,
             Address area_end, VirtualMemory reservation,
             Executability executability,
             MemoryChunk::MainThreadFlags* trusted_flags);

  V8_EXPORT_PRIVATE void MarkNeverAllocateForTesting();
  void MarkEvacuationCandidate();
  void ClearEvacuationCandidate();
  void AbortEvacuation();

  NormalPage* next_page() {
    return static_cast<NormalPage*>(list_node_.next());
  }
  NormalPage* prev_page() {
    return static_cast<NormalPage*>(list_node_.prev());
  }

  const NormalPage* next_page() const {
    return static_cast<const NormalPage*>(list_node_.next());
  }
  const NormalPage* prev_page() const {
    return static_cast<const NormalPage*>(list_node_.prev());
  }

  template <typename Callback>
  inline void ForAllFreeListCategories(Callback callback);

  V8_EXPORT_PRIVATE size_t AvailableInFreeList();

  size_t AvailableInFreeListFromAllocatedBytes() {
    DCHECK_GE(area_size(), wasted_memory() + allocated_bytes());
    return area_size() - wasted_memory() - allocated_bytes();
  }

  FreeListCategory* free_list_category(FreeListCategoryType type) {
    return categories_[type];
  }

  V8_EXPORT_PRIVATE void CreateBlackArea(Address start, Address end);
  void DestroyBlackArea(Address start, Address end);
  void ClearBlackAllocation();

  void InitializeFreeListCategories();
  void AllocateFreeListCategories();
  void ReleaseFreeListCategories();

  ActiveSystemPages* active_system_pages() {
    return active_system_pages_.get();
  }

  template <RememberedSetType remembered_set>
  void ClearTypedSlotsInFreeMemory(const TypedSlotSet::FreeRangesMap& ranges) {
    TypedSlotSet* typed_slot_set = this->typed_slot_set<remembered_set>();
    if (typed_slot_set != nullptr) {
      typed_slot_set->ClearInvalidSlots(ranges);
    }
  }

  template <RememberedSetType remembered_set>
  void AssertNoTypedSlotsInFreeMemory(
      const TypedSlotSet::FreeRangesMap& ranges) {
#if DEBUG
    TypedSlotSet* typed_slot_set = this->typed_slot_set<remembered_set>();
    if (typed_slot_set != nullptr) {
      typed_slot_set->AssertNoInvalidSlots(ranges);
    }
#endif  // DEBUG
  }

 private:
  friend class MemoryAllocator;
};

template <>
struct CastTraits<NormalPage> {
  static inline bool AllowFrom(const BasePage& page) {
    return page.IsNormalPage();
  }
};

}  // namespace internal

namespace base {
// Define special hash function for page pointers, to be used with std data
// structures, e.g. std::unordered_set<NormalPage*, base::hash<NormalPage*>
template <>
struct hash<i::NormalPage*> : hash<i::BasePage*> {};
template <>
struct hash<const i::NormalPage*> : hash<const i::BasePage*> {};
}  // namespace base

}  // namespace v8

#endif  // V8_HEAP_NORMAL_PAGE_H_
