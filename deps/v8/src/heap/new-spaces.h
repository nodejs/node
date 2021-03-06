// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NEW_SPACES_H_
#define V8_HEAP_NEW_SPACES_H_

#include <atomic>
#include <map>
#include <memory>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"
#include "src/logging/log.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

class Heap;
class MemoryChunk;

enum SemiSpaceId { kFromSpace = 0, kToSpace = 1 };

using ParkedAllocationBuffer = std::pair<int, Address>;
using ParkedAllocationBuffersVector = std::vector<ParkedAllocationBuffer>;

// -----------------------------------------------------------------------------
// SemiSpace in young generation
//
// A SemiSpace is a contiguous chunk of memory holding page-like memory chunks.
// The mark-compact collector  uses the memory of the first page in the from
// space as a marking stack when tracing live objects.
class SemiSpace : public Space {
 public:
  using iterator = PageIterator;
  using const_iterator = ConstPageIterator;

  static void Swap(SemiSpace* from, SemiSpace* to);

  SemiSpace(Heap* heap, SemiSpaceId semispace)
      : Space(heap, NEW_SPACE, new NoFreeList()),
        current_capacity_(0),
        target_capacity_(0),
        maximum_capacity_(0),
        minimum_capacity_(0),
        age_mark_(kNullAddress),
        id_(semispace),
        current_page_(nullptr) {}

  inline bool Contains(HeapObject o) const;
  inline bool Contains(Object o) const;
  inline bool ContainsSlow(Address a) const;

  void SetUp(size_t initial_capacity, size_t maximum_capacity);
  void TearDown();

  bool Commit();
  bool Uncommit();
  bool IsCommitted() { return !memory_chunk_list_.Empty(); }

  // Grow the semispace to the new capacity.  The new capacity requested must
  // be larger than the current capacity and less than the maximum capacity.
  bool GrowTo(size_t new_capacity);

  // Shrinks the semispace to the new capacity.  The new capacity requested
  // must be more than the amount of used memory in the semispace and less
  // than the current capacity.
  void ShrinkTo(size_t new_capacity);

  bool EnsureCurrentCapacity();

  // Returns the start address of the first page of the space.
  Address space_start() {
    DCHECK_NE(memory_chunk_list_.front(), nullptr);
    return memory_chunk_list_.front()->area_start();
  }

  Page* current_page() { return current_page_; }

  // Returns the start address of the current page of the space.
  Address page_low() { return current_page_->area_start(); }

  // Returns one past the end address of the current page of the space.
  Address page_high() { return current_page_->area_end(); }

  bool AdvancePage() {
    Page* next_page = current_page_->next_page();
    // We cannot expand if we reached the target capcity. Note
    // that we need to account for the next page already for this check as we
    // could potentially fill the whole page after advancing.
    if (next_page == nullptr || (current_capacity_ == target_capacity_)) {
      return false;
    }
    current_page_ = next_page;
    current_capacity_ += Page::kPageSize;
    return true;
  }

  // Resets the space to using the first page.
  void Reset();

  void RemovePage(Page* page);
  void PrependPage(Page* page);
  void MovePageToTheEnd(Page* page);

  Page* InitializePage(MemoryChunk* chunk);

  // Age mark accessors.
  Address age_mark() { return age_mark_; }
  void set_age_mark(Address mark);

  // Returns the current capacity of the semispace.
  size_t current_capacity() { return current_capacity_; }

  // Returns the target capacity of the semispace.
  size_t target_capacity() { return target_capacity_; }

  // Returns the maximum capacity of the semispace.
  size_t maximum_capacity() { return maximum_capacity_; }

  // Returns the initial capacity of the semispace.
  size_t minimum_capacity() { return minimum_capacity_; }

  SemiSpaceId id() { return id_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  // If we don't have these here then SemiSpace will be abstract.  However
  // they should never be called:

  size_t Size() override { UNREACHABLE(); }

  size_t SizeOfObjects() override { return Size(); }

  size_t Available() override { UNREACHABLE(); }

  Page* first_page() { return reinterpret_cast<Page*>(Space::first_page()); }
  Page* last_page() { return reinterpret_cast<Page*>(Space::last_page()); }

  const Page* first_page() const {
    return reinterpret_cast<const Page*>(Space::first_page());
  }

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  const_iterator begin() const { return const_iterator(first_page()); }
  const_iterator end() const { return const_iterator(nullptr); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

#ifdef DEBUG
  V8_EXPORT_PRIVATE void Print() override;
  // Validate a range of of addresses in a SemiSpace.
  // The "from" address must be on a page prior to the "to" address,
  // in the linked page order, or it must be earlier on the same page.
  static void AssertValidRange(Address from, Address to);
#else
  // Do nothing.
  inline static void AssertValidRange(Address from, Address to) {}
#endif

#ifdef VERIFY_HEAP
  virtual void Verify();
#endif

 private:
  void RewindPages(int num_pages);

  // Copies the flags into the masked positions on all pages in the space.
  void FixPagesFlags(intptr_t flags, intptr_t flag_mask);

  // The currently committed space capacity.
  size_t current_capacity_;

  // The targetted committed space capacity.
  size_t target_capacity_;

  // The maximum capacity that can be used by this space. A space cannot grow
  // beyond that size.
  size_t maximum_capacity_;

  // The minimum capacity for the space. A space cannot shrink below this size.
  size_t minimum_capacity_;

  // Used to govern object promotion during mark-compact collection.
  Address age_mark_;

  SemiSpaceId id_;

  Page* current_page_;

  friend class NewSpace;
  friend class SemiSpaceObjectIterator;
};

// A SemiSpaceObjectIterator is an ObjectIterator that iterates over the active
// semispace of the heap's new space.  It iterates over the objects in the
// semispace from a given start address (defaulting to the bottom of the
// semispace) to the top of the semispace.  New objects allocated after the
// iterator is created are not iterated.
class SemiSpaceObjectIterator : public ObjectIterator {
 public:
  // Create an iterator over the allocated objects in the given to-space.
  explicit SemiSpaceObjectIterator(NewSpace* space);

  inline HeapObject Next() override;

 private:
  void Initialize(Address start, Address end);

  // The current iteration point.
  Address current_;
  // The end of iteration.
  Address limit_;
};

// -----------------------------------------------------------------------------
// The young generation space.
//
// The new space consists of a contiguous pair of semispaces.  It simply
// forwards most functions to the appropriate semispace.

class V8_EXPORT_PRIVATE NewSpace
    : NON_EXPORTED_BASE(public SpaceWithLinearArea) {
 public:
  using iterator = PageIterator;
  using const_iterator = ConstPageIterator;

  NewSpace(Heap* heap, v8::PageAllocator* page_allocator,
           size_t initial_semispace_capacity, size_t max_semispace_capacity);

  ~NewSpace() override { TearDown(); }

  inline bool ContainsSlow(Address a) const;
  inline bool Contains(Object o) const;
  inline bool Contains(HeapObject o) const;

  // Tears down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  void TearDown();

  void ResetParkedAllocationBuffers();

  // Flip the pair of spaces.
  void Flip();

  // Grow the capacity of the semispaces.  Assumes that they are not at
  // their maximum capacity.
  void Grow();

  // Shrink the capacity of the semispaces.
  void Shrink();

  // Return the allocated bytes in the active semispace.
  size_t Size() final {
    DCHECK_GE(top(), to_space_.page_low());
    return (to_space_.current_capacity() - Page::kPageSize) / Page::kPageSize *
               MemoryChunkLayout::AllocatableMemoryInDataPage() +
           static_cast<size_t>(top() - to_space_.page_low());
  }

  size_t SizeOfObjects() final { return Size(); }

  // Return the allocatable capacity of a semispace.
  size_t Capacity() {
    SLOW_DCHECK(to_space_.target_capacity() == from_space_.target_capacity());
    return (to_space_.target_capacity() / Page::kPageSize) *
           MemoryChunkLayout::AllocatableMemoryInDataPage();
  }

  // Return the current size of a semispace, allocatable and non-allocatable
  // memory.
  size_t TotalCapacity() {
    DCHECK(to_space_.target_capacity() == from_space_.target_capacity());
    return to_space_.target_capacity();
  }

  // Committed memory for NewSpace is the committed memory of both semi-spaces
  // combined.
  size_t CommittedMemory() final {
    return from_space_.CommittedMemory() + to_space_.CommittedMemory();
  }

  size_t MaximumCommittedMemory() final {
    return from_space_.MaximumCommittedMemory() +
           to_space_.MaximumCommittedMemory();
  }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() final;

  // Return the available bytes without growing.
  size_t Available() final {
    DCHECK_GE(Capacity(), Size());
    return Capacity() - Size();
  }

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) const final {
    if (type == ExternalBackingStoreType::kArrayBuffer)
      return heap()->YoungArrayBufferBytes();
    DCHECK_EQ(0, from_space_.ExternalBackingStoreBytes(type));
    return to_space_.ExternalBackingStoreBytes(type);
  }

  size_t ExternalBackingStoreBytes() {
    size_t result = 0;
    for (int i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
      result +=
          ExternalBackingStoreBytes(static_cast<ExternalBackingStoreType>(i));
    }
    return result;
  }

  size_t AllocatedSinceLastGC() {
    const Address age_mark = to_space_.age_mark();
    DCHECK_NE(age_mark, kNullAddress);
    DCHECK_NE(top(), kNullAddress);
    Page* const age_mark_page = Page::FromAllocationAreaAddress(age_mark);
    Page* const last_page = Page::FromAllocationAreaAddress(top());
    Page* current_page = age_mark_page;
    size_t allocated = 0;
    if (current_page != last_page) {
      DCHECK_EQ(current_page, age_mark_page);
      DCHECK_GE(age_mark_page->area_end(), age_mark);
      allocated += age_mark_page->area_end() - age_mark;
      current_page = current_page->next_page();
    } else {
      DCHECK_GE(top(), age_mark);
      return top() - age_mark;
    }
    while (current_page != last_page) {
      DCHECK_NE(current_page, age_mark_page);
      allocated += MemoryChunkLayout::AllocatableMemoryInDataPage();
      current_page = current_page->next_page();
    }
    DCHECK_GE(top(), current_page->area_start());
    allocated += top() - current_page->area_start();
    DCHECK_LE(allocated, Size());
    return allocated;
  }

  void MovePageFromSpaceToSpace(Page* page) {
    DCHECK(page->IsFromPage());
    from_space_.RemovePage(page);
    to_space_.PrependPage(page);
  }

  bool Rebalance();

  // Return the maximum capacity of a semispace.
  size_t MaximumCapacity() {
    DCHECK(to_space_.maximum_capacity() == from_space_.maximum_capacity());
    return to_space_.maximum_capacity();
  }

  bool IsAtMaximumCapacity() { return TotalCapacity() == MaximumCapacity(); }

  // Returns the initial capacity of a semispace.
  size_t InitialTotalCapacity() {
    DCHECK(to_space_.minimum_capacity() == from_space_.minimum_capacity());
    return to_space_.minimum_capacity();
  }

  void VerifyTop();

  Address original_top_acquire() {
    return original_top_.load(std::memory_order_acquire);
  }
  Address original_limit_relaxed() {
    return original_limit_.load(std::memory_order_relaxed);
  }

  // Return the address of the first allocatable address in the active
  // semispace. This may be the address where the first object resides.
  Address first_allocatable_address() { return to_space_.space_start(); }

  // Get the age mark of the inactive semispace.
  Address age_mark() { return from_space_.age_mark(); }
  // Set the age mark in the active semispace.
  void set_age_mark(Address mark) { to_space_.set_age_mark(mark); }

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationAlignment alignment,
              AllocationOrigin origin = AllocationOrigin::kRuntime);

  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawSynchronized(
      int size_in_bytes, AllocationAlignment alignment,
      AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Reset the allocation pointer to the beginning of the active semispace.
  void ResetLinearAllocationArea();

  // When inline allocation stepping is active, either because of incremental
  // marking, idle scavenge, or allocation statistics gathering, we 'interrupt'
  // inline allocation every once in a while. This is done by setting
  // allocation_info_.limit to be lower than the actual limit and and increasing
  // it in steps to guarantee that the observers are notified periodically.
  void UpdateInlineAllocationLimit(size_t size_in_bytes) override;

  inline bool ToSpaceContainsSlow(Address a) const;
  inline bool ToSpaceContains(Object o) const;
  inline bool FromSpaceContains(Object o) const;

  // Try to switch the active semispace to a new, empty, page.
  // Returns false if this isn't possible or reasonable (i.e., there
  // are no pages, or the current page is already empty), or true
  // if successful.
  bool AddFreshPage();
  bool AddFreshPageSynchronized();

  bool AddParkedAllocationBuffer(int size_in_bytes,
                                 AllocationAlignment alignment);

#ifdef VERIFY_HEAP
  // Verify the active semispace.
  virtual void Verify(Isolate* isolate);
#endif

#ifdef DEBUG
  // Print the active semispace.
  void Print() override { to_space_.Print(); }
#endif

  // Return whether the operation succeeded.
  bool CommitFromSpaceIfNeeded() {
    if (from_space_.IsCommitted()) return true;
    return from_space_.Commit();
  }

  bool UncommitFromSpace() {
    if (!from_space_.IsCommitted()) return true;
    return from_space_.Uncommit();
  }

  bool IsFromSpaceCommitted() { return from_space_.IsCommitted(); }

  SemiSpace* active_space() { return &to_space_; }

  Page* first_page() { return to_space_.first_page(); }
  Page* last_page() { return to_space_.last_page(); }

  iterator begin() { return to_space_.begin(); }
  iterator end() { return to_space_.end(); }

  const_iterator begin() const { return to_space_.begin(); }
  const_iterator end() const { return to_space_.end(); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

  SemiSpace& from_space() { return from_space_; }
  SemiSpace& to_space() { return to_space_; }

  void MoveOriginalTopForward() {
    DCHECK_GE(top(), original_top_);
    DCHECK_LE(top(), original_limit_);
    original_top_.store(top(), std::memory_order_release);
  }

  void MaybeFreeUnusedLab(LinearAllocationArea info);

 private:
  static const int kAllocationBufferParkingThreshold = 4 * KB;

  // Update linear allocation area to match the current to-space page.
  void UpdateLinearAllocationArea(Address known_top = 0);

  base::Mutex mutex_;

  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks.
  std::atomic<Address> original_top_;
  std::atomic<Address> original_limit_;

  // The semispaces.
  SemiSpace to_space_;
  SemiSpace from_space_;
  VirtualMemory reservation_;

  ParkedAllocationBuffersVector parked_allocation_buffers_;

  // Internal allocation methods.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateFastAligned(int size_in_bytes, int* aligned_size_in_bytes,
                      AllocationAlignment alignment, AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateFastUnaligned(int size_in_bytes, AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRawSlow(int size_in_bytes, AllocationAlignment alignment,
                  AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRawAligned(int size_in_bytes, AllocationAlignment alignment,
                     AllocationOrigin origin = AllocationOrigin::kRuntime);

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRawUnaligned(
      int size_in_bytes, AllocationOrigin origin = AllocationOrigin::kRuntime);

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment);
  bool SupportsAllocationObserver() override { return true; }

  friend class SemiSpaceObjectIterator;
};

// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define DCHECK_SEMISPACE_ALLOCATION_INFO(info, space) \
  SLOW_DCHECK((space).page_low() <= (info).top() &&   \
              (info).top() <= (space).page_high() &&  \
              (info).limit() <= (space).page_high())

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_NEW_SPACES_H_
