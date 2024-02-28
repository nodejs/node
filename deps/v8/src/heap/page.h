// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGE_H_
#define V8_HEAP_PAGE_H_

#include "src/heap/base-space.h"
#include "src/heap/free-list.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

class Heap;

// -----------------------------------------------------------------------------
// A page is a memory chunk of a size 256K. Large object pages may be larger.
//
// The only way to get a page pointer is by calling factory methods:
//   Page* p = Page::FromAddress(addr); or
//   Page* p = Page::FromAllocationAreaAddress(address);
class Page : public MemoryChunk {
 public:
  // Page flags copied from from-space to to-space when flipping semispaces.
  static constexpr MainThreadFlags kCopyOnFlipFlagsMask =
      MainThreadFlags(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING) |
      MainThreadFlags(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING) |
      MainThreadFlags(MemoryChunk::INCREMENTAL_MARKING);

  Page(Heap* heap, BaseSpace* space, size_t size, Address area_start,
       Address area_end, VirtualMemory reservation, Executability executable);

  // Returns the page containing a given address. The address ranges
  // from [page_addr .. page_addr + kPageSize]. This only works if the object
  // is in fact in a page.
  static Page* FromAddress(Address addr) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<Page*>(addr & ~kPageAlignmentMask);
  }
  static Page* FromHeapObject(Tagged<HeapObject> o) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<Page*>(o.ptr() & ~kAlignmentMask);
  }

  static Page* cast(BasicMemoryChunk* chunk) {
    return cast(MemoryChunk::cast(chunk));
  }

  static Page* cast(MemoryChunk* chunk) {
    DCHECK_IMPLIES(chunk, !chunk->IsLargePage());
    return static_cast<Page*>(chunk);
  }

  // Returns the page containing the address provided. The address can
  // potentially point righter after the page. To be also safe for tagged values
  // we subtract a hole word. The valid address ranges from
  // [page_addr + area_start_ .. page_addr + kPageSize + kTaggedSize].
  static Page* FromAllocationAreaAddress(Address address) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return Page::FromAddress(address - kTaggedSize);
  }

  // Checks if address1 and address2 are on the same new space page.
  static bool OnSamePage(Address address1, Address address2) {
    return Page::FromAddress(address1) == Page::FromAddress(address2);
  }

  // Checks whether an address is page aligned.
  static bool IsAlignedToPageSize(Address addr) {
    return (addr & kPageAlignmentMask) == 0;
  }

  static Page* ConvertNewToOld(Page* old_page);

  V8_EXPORT_PRIVATE void MarkNeverAllocateForTesting();
  inline void MarkEvacuationCandidate();
  inline void ClearEvacuationCandidate();

  Page* next_page() { return static_cast<Page*>(list_node_.next()); }
  Page* prev_page() { return static_cast<Page*>(list_node_.prev()); }

  const Page* next_page() const {
    return static_cast<const Page*>(list_node_.next());
  }
  const Page* prev_page() const {
    return static_cast<const Page*>(list_node_.prev());
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

  V8_EXPORT_PRIVATE size_t ShrinkToHighWaterMark();

  V8_EXPORT_PRIVATE void CreateBlackArea(Address start, Address end);
  void DestroyBlackArea(Address start, Address end);

  void InitializeFreeListCategories();
  void AllocateFreeListCategories();
  void ReleaseFreeListCategories();

  ActiveSystemPages* active_system_pages() { return active_system_pages_; }

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

// Validate our estimates on the header size.
static_assert(sizeof(BasicMemoryChunk) <= BasicMemoryChunk::kHeaderSize);
static_assert(sizeof(MemoryChunk) <= MemoryChunk::kHeaderSize);
static_assert(sizeof(Page) <= MemoryChunk::kHeaderSize);

}  // namespace internal

namespace base {
// Define special hash function for page pointers, to be used with std data
// structures, e.g. std::unordered_set<Page*, base::hash<Page*>
template <>
struct hash<i::Page*> : hash<i::BasicMemoryChunk*> {};
template <>
struct hash<const i::Page*> : hash<const i::BasicMemoryChunk*> {};
}  // namespace base

}  // namespace v8

#endif  // V8_HEAP_PAGE_H_
