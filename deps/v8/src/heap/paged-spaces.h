// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGED_SPACES_H_
#define V8_HEAP_PAGED_SPACES_H_

#include <atomic>
#include <limits>
#include <memory>
#include <utility>

#include "src/base/bounds.h"
#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/allocation-stats.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/heap.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

class CompactionSpace;
class Heap;
class HeapObject;
class Isolate;
class ObjectVisitor;
class PagedSpaceBase;
class Sweeper;

class HeapObjectRange final {
 public:
  class iterator final {
   public:
    using value_type = HeapObject;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::forward_iterator_tag;

    inline iterator();
    explicit inline iterator(const Page* page);

    inline iterator& operator++();
    inline iterator operator++(int);

    bool operator==(iterator other) const {
      return cur_addr_ == other.cur_addr_;
    }
    bool operator!=(iterator other) const { return !(*this == other); }

    value_type operator*() { return HeapObject::FromAddress(cur_addr_); }

   private:
    inline void AdvanceToNextObject();

    PtrComprCageBase cage_base() const { return cage_base_; }

    PtrComprCageBase cage_base_;
    Address cur_addr_ = kNullAddress;  // Current iteration point.
    int cur_size_ = 0;
    Address cur_end_ = kNullAddress;  // End iteration point.
  };

  explicit HeapObjectRange(const Page* page) : page_(page) {}

  inline iterator begin();
  inline iterator end();

 private:
  const Page* const page_;
};

// Heap object iterator in paged spaces.
//
// A PagedSpaceObjectIterator iterates objects from the bottom of the given
// space to its top or from the bottom of the given page to its top.
//
// If objects are allocated in the page during iteration the iterator may
// or may not iterate over those objects.  The caller must create a new
// iterator in order to be sure to visit these new objects.
class V8_EXPORT_PRIVATE PagedSpaceObjectIterator : public ObjectIterator {
 public:
  // Creates a new object iterator in a given space.
  PagedSpaceObjectIterator(Heap* heap, const PagedSpaceBase* space);

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns nullptr when the iteration has ended.
  inline Tagged<HeapObject> Next() override;

 private:
  // Slow path of next(), goes into the next page.  Returns false if the
  // iteration has ended.
  bool AdvanceToNextPage();

  HeapObjectRange::iterator cur_;
  HeapObjectRange::iterator end_;
  const PagedSpaceBase* const space_;
  ConstPageRange page_range_;
  ConstPageRange::iterator current_page_;
};

class V8_EXPORT_PRIVATE PagedSpaceBase
    : NON_EXPORTED_BASE(public SpaceWithLinearArea) {
 public:
  using iterator = PageIterator;
  using const_iterator = ConstPageIterator;

  static const size_t kCompactionMemoryWanted = 500 * KB;

  // Creates a space with an id.
  PagedSpaceBase(
      Heap* heap, AllocationSpace id, Executability executable,
      std::unique_ptr<FreeList> free_list,
      AllocationCounter& allocation_counter,
      LinearAllocationArea& allocation_info,
      LinearAreaOriginalData& linear_area_original_data,
      CompactionSpaceKind compaction_space_kind = CompactionSpaceKind::kNone);

  ~PagedSpaceBase() override { TearDown(); }

  // Checks whether an object/address is in this space.
  inline bool Contains(Address a) const;
  inline bool Contains(Tagged<Object> o) const;
  bool ContainsSlow(Address addr) const;

  // Does the space need executable memory?
  Executability executable() const { return executable_; }

  // Current capacity without growing (Size() + Available()).
  size_t Capacity() const { return accounting_stats_.Capacity(); }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() const override;

#if DEBUG
  void VerifyCommittedPhysicalMemory() const;
#endif  // DEBUG

  void IncrementCommittedPhysicalMemory(size_t increment_value);
  void DecrementCommittedPhysicalMemory(size_t decrement_value);

  // Sets the capacity, the available space and the wasted space to zero.
  // The stats are rebuilt during sweeping by adding each page to the
  // capacity and the size when it is encountered.  As free spaces are
  // discovered during the sweeping they are subtracted from the size and added
  // to the available and wasted totals. The free list is cleared as well.
  void ClearAllocatorState() {
    accounting_stats_.ClearSize();
    free_list_->Reset();
  }

  // Available bytes without growing.  These are the bytes on the free list.
  // The bytes in the linear allocation area are not included in this total
  // because updating the stats would slow down allocation.  New pages are
  // immediately added to the free list so they show up here.
  size_t Available() const override;

  // Allocated bytes in this space.  Garbage bytes that were not found due to
  // concurrent sweeping are counted as being allocated!  The bytes in the
  // current linear allocation area (between top and limit) are also counted
  // here.
  size_t Size() const override { return accounting_stats_.Size(); }

  // Wasted bytes in this space.  These are just the bytes that were thrown away
  // due to being too small to use for allocation.
  virtual size_t Waste() const { return free_list_->wasted_bytes(); }

  // Allocate the requested number of bytes in the space from a background
  // thread.
  V8_WARN_UNUSED_RESULT base::Optional<std::pair<Address, size_t>>
  RawAllocateBackground(LocalHeap* local_heap, size_t min_size_in_bytes,
                        size_t max_size_in_bytes, AllocationOrigin origin);

  size_t Free(Address start, size_t size_in_bytes, SpaceAccountingMode mode) {
    if (size_in_bytes == 0) return 0;
    heap()->CreateFillerObjectAtBackground(start,
                                           static_cast<int>(size_in_bytes));
    if (mode == SpaceAccountingMode::kSpaceAccounted) {
      return AccountedFree(start, size_in_bytes);
    } else {
      return UnaccountedFree(start, size_in_bytes);
    }
  }

  // Give a block of memory to the space's free list.  It might be added to
  // the free list or accounted as waste.
  // If add_to_freelist is false then just accounting stats are updated and
  // no attempt to add area to free list is made.
  size_t AccountedFree(Address start, size_t size_in_bytes) {
    size_t wasted = free_list_->Free(start, size_in_bytes, kLinkCategory);
    Page* page = Page::FromAddress(start);
    accounting_stats_.DecreaseAllocatedBytes(size_in_bytes, page);
    free_list()->increase_wasted_bytes(wasted);
    DCHECK_GE(size_in_bytes, wasted);
    return size_in_bytes - wasted;
  }

  size_t UnaccountedFree(Address start, size_t size_in_bytes) {
    size_t wasted = free_list_->Free(start, size_in_bytes, kDoNotLinkCategory);
    DCHECK_GE(size_in_bytes, wasted);
    return size_in_bytes - wasted;
  }

  inline bool TryFreeLast(Address object_address, int object_size);

  void ResetFreeList();

  // Empty space linear allocation area, returning unused area to free list.
  void FreeLinearAllocationArea() override;

  void MakeLinearAllocationAreaIterable();

  void MarkLinearAllocationAreaBlack();
  void UnmarkLinearAllocationArea();

  void DecreaseAllocatedBytes(size_t bytes, Page* page) {
    accounting_stats_.DecreaseAllocatedBytes(bytes, page);
  }
  void IncreaseAllocatedBytes(size_t bytes, Page* page) {
    accounting_stats_.IncreaseAllocatedBytes(bytes, page);
  }
  void DecreaseCapacity(size_t bytes) {
    accounting_stats_.DecreaseCapacity(bytes);
  }
  void IncreaseCapacity(size_t bytes) {
    accounting_stats_.IncreaseCapacity(bytes);
  }

  Page* InitializePage(MemoryChunk* chunk) override;

  virtual void ReleasePage(Page* page);

  // Adds the page to this space and returns the number of bytes added to the
  // free list of the space.
  virtual size_t AddPage(Page* page);
  virtual void RemovePage(Page* page);
  // Remove a page if it has at least |size_in_bytes| bytes available that can
  // be used for allocation.
  Page* RemovePageSafe(int size_in_bytes);

  void SetReadable();
  void SetReadAndExecutable();

  void SetDefaultCodePermissions() {
    if (v8_flags.jitless) {
      SetReadable();
    } else {
      SetReadAndExecutable();
    }
  }

#ifdef VERIFY_HEAP
  // Verify integrity of this space.
  void Verify(Isolate* isolate,
              SpaceVerificationVisitor* visitor) const override;

  void VerifyLiveBytes() const;
#endif

#ifdef DEBUG
  void VerifyCountersAfterSweeping(Heap* heap) const;
  void VerifyCountersBeforeConcurrentSweeping() const;
  // Print meta info and objects in this space.
  void Print() override;

  // Report code object related statistics
  static void ReportCodeStatistics(Isolate* isolate);
  static void ResetCodeStatistics(Isolate* isolate);
#endif

  bool CanExpand(size_t size) const;

  // Returns the number of total pages in this space.
  int CountTotalPages() const;

  // Return size of allocatable area on a page in this space.
  inline int AreaSize() const { return static_cast<int>(area_size_); }

  bool is_compaction_space() const {
    return compaction_space_kind_ != CompactionSpaceKind::kNone;
  }

  CompactionSpaceKind compaction_space_kind() const {
    return compaction_space_kind_;
  }

  // Merges {other} into the current space. Note that this modifies {other},
  // e.g., removes its bump pointer area and resets statistics.
  void MergeCompactionSpace(CompactionSpace* other);

  // Refills the free list from the corresponding free list filled by the
  // sweeper.
  virtual void RefillFreeList();

  base::Mutex* mutex() { return &space_mutex_; }

  void UnlinkFreeListCategories(Page* page);
  size_t RelinkFreeListCategories(Page* page);

  Page* first_page() override {
    return reinterpret_cast<Page*>(memory_chunk_list_.front());
  }
  const Page* first_page() const override {
    return reinterpret_cast<const Page*>(memory_chunk_list_.front());
  }

  Page* last_page() override {
    return reinterpret_cast<Page*>(memory_chunk_list_.back());
  }
  const Page* last_page() const override {
    return reinterpret_cast<const Page*>(memory_chunk_list_.back());
  }

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  const_iterator begin() const { return const_iterator(first_page()); }
  const_iterator end() const { return const_iterator(nullptr); }

  // Shrink immortal immovable pages of the space to be exactly the size needed
  // using the high water mark.
  void ShrinkImmortalImmovablePages();

  size_t ShrinkPageToHighWaterMark(Page* page);

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

  void SetLinearAllocationArea(Address top, Address limit, Address end);

  void AddRangeToActiveSystemPages(Page* page, Address start, Address end);
  void ReduceActiveSystemPages(Page* page,
                               ActiveSystemPages active_system_pages);

  // Allocates memory with the given size constraints from the space's free
  // list.
  V8_WARN_UNUSED_RESULT base::Optional<std::pair<Address, size_t>>
  TryAllocationFromFreeListBackground(size_t min_size_in_bytes,
                                      size_t max_size_in_bytes,
                                      AllocationOrigin origin);

  // Expands the space by a single page from a background thread and allocates
  // a memory area of the given size in it. If successful the method returns
  // the address and size of the area.
  base::Optional<std::pair<Address, size_t>> TryExpandBackground(
      size_t size_in_bytes);

  void RefineAllocatedBytesAfterSweeping(Page* page);

 protected:
  // Updates the current lab limit without updating top, original_top or
  // original_limit.
  void SetLimit(Address limit);

  bool SupportsExtendingLAB() const { return identity() == NEW_SPACE; }

  void UpdateInlineAllocationLimit() override;

  // PagedSpaces that should be included in snapshots have different, i.e.,
  // smaller, initial pages.
  virtual bool snapshotable() const { return true; }

  bool HasPages() const { return first_page() != nullptr; }

  // Cleans up the space, frees all pages in this space except those belonging
  // to the initial chunk, uncommits addresses in the initial chunk.
  void TearDown();

  // Expands the space by allocating a fixed number of pages. Returns false if
  // it cannot allocate requested number of pages from OS, or if the hard heap
  // size limit has been hit.
  virtual Page* TryExpandImpl(MemoryAllocator::AllocationMode allocation_mode);

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment,
                        AllocationOrigin origin,
                        int* out_max_aligned_size) override;

  V8_WARN_UNUSED_RESULT bool TryAllocationFromFreeListMain(
      size_t size_in_bytes, AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT bool ContributeToSweepingMain(
      int required_freed_bytes, int max_pages, int size_in_bytes,
      AllocationOrigin origin, GCTracer::Scope::ScopeId sweeping_scope_id,
      ThreadKind sweeping_scope_kind);

  // Refills LAB for EnsureLabMain. This function is space-dependent. Returns
  // false if there is not enough space and the caller has to retry after
  // collecting garbage.
  V8_WARN_UNUSED_RESULT virtual bool RefillLabMain(int size_in_bytes,
                                                   AllocationOrigin origin);

  // Actual implementation of refilling LAB. Returns false if there is not
  // enough space and the caller has to retry after collecting garbage.
  V8_WARN_UNUSED_RESULT bool RawRefillLabMain(int size_in_bytes,
                                              AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT bool TryExtendLAB(int size_in_bytes);

  V8_WARN_UNUSED_RESULT bool TryExpand(int size_in_bytes,
                                       AllocationOrigin origin);

  size_t committed_physical_memory() const {
    return committed_physical_memory_.load(std::memory_order_relaxed);
  }

  void ReleasePageImpl(Page* page, MemoryAllocator::FreeMode free_mode);

  void AddPageImpl(Page* page);

  Executability executable_;

  CompactionSpaceKind compaction_space_kind_;

  size_t area_size_;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  // Mutex guarding any concurrent access to the space.
  mutable base::Mutex space_mutex_;

  std::atomic<size_t> committed_physical_memory_{0};

  // Used for tracking bytes allocated since last gc in new space.
  size_t size_at_last_gc_ = 0;

 private:
  class ConcurrentAllocationMutex {
   public:
    explicit ConcurrentAllocationMutex(const PagedSpaceBase* space) {
      if (space->SupportsConcurrentAllocation()) {
        guard_.emplace(&space->space_mutex_);
      }
    }

    base::Optional<base::MutexGuard> guard_;
  };

  bool SupportsConcurrentAllocation() const {
    return !is_compaction_space() && (identity() != NEW_SPACE);
  }

  // Set space linear allocation area.
  void SetTopAndLimit(Address top, Address limit, Address end);
  void DecreaseLimit(Address new_limit);
  bool SupportsAllocationObserver() const override {
    return !is_compaction_space();
  }

  friend class IncrementalMarking;
  friend class MarkCompactCollector;

  // Used in cctest.
  friend class heap::HeapTester;
};

class V8_EXPORT_PRIVATE PagedSpace : public PagedSpaceBase {
 public:
  // Creates a space with an id.
  PagedSpace(
      Heap* heap, AllocationSpace id, Executability executable,
      std::unique_ptr<FreeList> free_list,
      LinearAllocationArea& allocation_info,
      CompactionSpaceKind compaction_space_kind = CompactionSpaceKind::kNone)
      : PagedSpaceBase(heap, id, executable, std::move(free_list),
                       allocation_counter_, allocation_info,
                       linear_area_original_data_, compaction_space_kind) {}

 private:
  AllocationCounter allocation_counter_;
  LinearAreaOriginalData linear_area_original_data_;
};

// -----------------------------------------------------------------------------
// Compaction space that is used temporarily during compaction.

class V8_EXPORT_PRIVATE CompactionSpace final : public PagedSpace {
 public:
  CompactionSpace(Heap* heap, AllocationSpace id, Executability executable,
                  CompactionSpaceKind compaction_space_kind)
      : PagedSpace(heap, id, executable, FreeList::CreateFreeList(),
                   allocation_info_, compaction_space_kind) {
    DCHECK(is_compaction_space());
  }

  const std::vector<Page*>& GetNewPages() { return new_pages_; }

  void RefillFreeList() final;

 protected:
  V8_WARN_UNUSED_RESULT bool RefillLabMain(int size_in_bytes,
                                           AllocationOrigin origin) final;

  Page* TryExpandImpl(MemoryAllocator::AllocationMode allocation_mode) final;
  // The space is temporary and not included in any snapshots.
  bool snapshotable() const final { return false; }
  // Pages that were allocated in this local space and need to be merged
  // to the main space.
  std::vector<Page*> new_pages_;

 private:
  LinearAllocationArea allocation_info_;
};

// A collection of |CompactionSpace|s used by a single compaction task.
class CompactionSpaceCollection : public Malloced {
 public:
  explicit CompactionSpaceCollection(Heap* heap,
                                     CompactionSpaceKind compaction_space_kind)
      : old_space_(heap, OLD_SPACE, Executability::NOT_EXECUTABLE,
                   compaction_space_kind),
        code_space_(heap, CODE_SPACE, Executability::EXECUTABLE,
                    compaction_space_kind),
        shared_space_(heap, SHARED_SPACE, Executability::NOT_EXECUTABLE,
                      compaction_space_kind) {}

  CompactionSpace* Get(AllocationSpace space) {
    switch (space) {
      case OLD_SPACE:
        return &old_space_;
      case CODE_SPACE:
        return &code_space_;
      case SHARED_SPACE:
        return &shared_space_;
      default:
        UNREACHABLE();
    }
    UNREACHABLE();
  }

 private:
  CompactionSpace old_space_;
  CompactionSpace code_space_;
  CompactionSpace shared_space_;
};

// -----------------------------------------------------------------------------
// Old generation regular object space.

class OldSpace final : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  explicit OldSpace(Heap* heap, LinearAllocationArea& allocation_info)
      : PagedSpace(heap, OLD_SPACE, NOT_EXECUTABLE, FreeList::CreateFreeList(),
                   allocation_info) {}

  static bool IsAtPageStart(Address addr) {
    return static_cast<intptr_t>(addr & kPageAlignmentMask) ==
           MemoryChunkLayout::ObjectStartOffsetInDataPage();
  }

  void AddPromotedPage(Page* page);

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) const final {
    if (type == ExternalBackingStoreType::kArrayBuffer)
      return heap()->OldArrayBufferBytes();
    return external_backing_store_bytes_[static_cast<int>(type)];
  }
};

// -----------------------------------------------------------------------------
// Old generation code object space.

class CodeSpace final : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  explicit CodeSpace(Heap* heap)
      : PagedSpace(heap, CODE_SPACE, EXECUTABLE, FreeList::CreateFreeList(),
                   paged_allocation_info_) {}

 private:
  LinearAllocationArea paged_allocation_info_;
};

// -----------------------------------------------------------------------------
// Shared space regular object space.

class SharedSpace final : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  explicit SharedSpace(Heap* heap)
      : PagedSpace(heap, SHARED_SPACE, NOT_EXECUTABLE,
                   FreeList::CreateFreeList(), allocation_info) {}

  static bool IsAtPageStart(Address addr) {
    return static_cast<intptr_t>(addr & kPageAlignmentMask) ==
           MemoryChunkLayout::ObjectStartOffsetInDataPage();
  }

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) const final {
    if (type == ExternalBackingStoreType::kArrayBuffer) return 0;
    DCHECK_EQ(type, ExternalBackingStoreType::kExternalString);
    return external_backing_store_bytes_[static_cast<int>(type)];
  }

 private:
  LinearAllocationArea allocation_info;
};

// Iterates over the chunks (pages and large object pages) that can contain
// pointers to new space or to evacuation candidates.
class OldGenerationMemoryChunkIterator {
 public:
  inline explicit OldGenerationMemoryChunkIterator(Heap* heap);

  // Return nullptr when the iterator is done.
  inline MemoryChunk* next();

  // Applies `callback` to all `MemoryChunk` returned by the iterator.
  template <typename Callback>
  static void ForAll(Heap* heap, Callback callback) {
    OldGenerationMemoryChunkIterator it(heap);
    MemoryChunk* chunk;
    while ((chunk = it.next()) != nullptr) {
      callback(chunk);
    }
  }

 private:
  enum State {
    kOldSpaceState,
    kCodeState,
    kLargeObjectState,
    kCodeLargeObjectState,
    kFinishedState
  };
  Heap* const heap_;
  State state_;
  PageIterator old_iterator_;
  PageIterator code_iterator_;
  LargePageIterator lo_iterator_;
  LargePageIterator code_lo_iterator_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGED_SPACES_H_
