// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGED_SPACES_H_
#define V8_HEAP_PAGED_SPACES_H_

#include <memory>
#include <utility>

#include "src/base/bounds.h"
#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/allocation-stats.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

class Heap;
class HeapObject;
class Isolate;
class LocalSpace;
class ObjectVisitor;

// -----------------------------------------------------------------------------
// Heap object iterator in old/map spaces.
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
  PagedSpaceObjectIterator(Heap* heap, PagedSpace* space);
  PagedSpaceObjectIterator(Heap* heap, PagedSpace* space, Page* page);

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns nullptr when the iteration has ended.
  inline HeapObject Next() override;

 private:
  // Fast (inlined) path of next().
  inline HeapObject FromCurrentPage();

  // Slow path of next(), goes into the next page.  Returns false if the
  // iteration has ended.
  bool AdvanceToNextPage();

  Address cur_addr_;  // Current iteration point.
  Address cur_end_;   // End iteration point.
  PagedSpace* space_;
  PageRange page_range_;
  PageRange::iterator current_page_;
};

class V8_EXPORT_PRIVATE PagedSpace
    : NON_EXPORTED_BASE(public SpaceWithLinearArea) {
 public:
  using iterator = PageIterator;
  using const_iterator = ConstPageIterator;

  static const size_t kCompactionMemoryWanted = 500 * KB;

  // Creates a space with an id.
  PagedSpace(Heap* heap, AllocationSpace id, Executability executable,
             FreeList* free_list,
             LocalSpaceKind local_space_kind = LocalSpaceKind::kNone);

  ~PagedSpace() override { TearDown(); }

  // Checks whether an object/address is in this space.
  inline bool Contains(Address a) const;
  inline bool Contains(Object o) const;
  bool ContainsSlow(Address addr) const;

  // Does the space need executable memory?
  Executability executable() { return executable_; }

  // Prepares for a mark-compact GC.
  void PrepareForMarkCompact();

  // Current capacity without growing (Size() + Available()).
  size_t Capacity() { return accounting_stats_.Capacity(); }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

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
  size_t Available() override;

  // Allocated bytes in this space.  Garbage bytes that were not found due to
  // concurrent sweeping are counted as being allocated!  The bytes in the
  // current linear allocation area (between top and limit) are also counted
  // here.
  size_t Size() override { return accounting_stats_.Size(); }

  // Wasted bytes in this space.  These are just the bytes that were thrown away
  // due to being too small to use for allocation.
  virtual size_t Waste() { return free_list_->wasted_bytes(); }

  // Allocate the requested number of bytes in the space if possible, return a
  // failure object if not.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawUnaligned(
      int size_in_bytes, AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Allocate the requested number of bytes in the space double aligned if
  // possible, return a failure object if not.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment,
      AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Allocate the requested number of bytes in the space and consider allocation
  // alignment if needed.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationAlignment alignment,
      AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Allocate the requested number of bytes in the space from a background
  // thread.
  V8_WARN_UNUSED_RESULT base::Optional<std::pair<Address, size_t>>
  RawRefillLabBackground(LocalHeap* local_heap, size_t min_size_in_bytes,
                         size_t max_size_in_bytes,
                         AllocationAlignment alignment,
                         AllocationOrigin origin);

  size_t Free(Address start, size_t size_in_bytes, SpaceAccountingMode mode) {
    if (size_in_bytes == 0) return 0;
    heap()->CreateFillerObjectAtBackground(
        start, static_cast<int>(size_in_bytes),
        ClearFreedMemoryMode::kDontClearFreedMemory);
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
    DCHECK_GE(size_in_bytes, wasted);
    return size_in_bytes - wasted;
  }

  size_t UnaccountedFree(Address start, size_t size_in_bytes) {
    size_t wasted = free_list_->Free(start, size_in_bytes, kDoNotLinkCategory);
    DCHECK_GE(size_in_bytes, wasted);
    return size_in_bytes - wasted;
  }

  inline bool TryFreeLast(HeapObject object, int object_size);

  void ResetFreeList();

  // Empty space linear allocation area, returning unused area to free list.
  void FreeLinearAllocationArea();

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

  void RefineAllocatedBytesAfterSweeping(Page* page);

  Page* InitializePage(MemoryChunk* chunk);

  void ReleasePage(Page* page);

  // Adds the page to this space and returns the number of bytes added to the
  // free list of the space.
  size_t AddPage(Page* page);
  void RemovePage(Page* page);
  // Remove a page if it has at least |size_in_bytes| bytes available that can
  // be used for allocation.
  Page* RemovePageSafe(int size_in_bytes);

  void SetReadable();
  void SetReadAndExecutable();
  void SetReadAndWritable();

  void SetDefaultCodePermissions() {
    if (FLAG_jitless) {
      SetReadable();
    } else {
      SetReadAndExecutable();
    }
  }

#ifdef VERIFY_HEAP
  // Verify integrity of this space.
  virtual void Verify(Isolate* isolate, ObjectVisitor* visitor);

  void VerifyLiveBytes();

  // Overridden by subclasses to verify space-specific object
  // properties (e.g., only maps or free-list nodes are in map space).
  virtual void VerifyObject(HeapObject obj) {}
#endif

#ifdef DEBUG
  void VerifyCountersAfterSweeping(Heap* heap);
  void VerifyCountersBeforeConcurrentSweeping();
  // Print meta info and objects in this space.
  void Print() override;

  // Report code object related statistics
  static void ReportCodeStatistics(Isolate* isolate);
  static void ResetCodeStatistics(Isolate* isolate);
#endif

  bool CanExpand(size_t size);

  // Returns the number of total pages in this space.
  int CountTotalPages();

  // Return size of allocatable area on a page in this space.
  inline int AreaSize() { return static_cast<int>(area_size_); }

  bool is_local_space() { return local_space_kind_ != LocalSpaceKind::kNone; }

  bool is_compaction_space() {
    return base::IsInRange(local_space_kind_,
                           LocalSpaceKind::kFirstCompactionSpace,
                           LocalSpaceKind::kLastCompactionSpace);
  }

  LocalSpaceKind local_space_kind() { return local_space_kind_; }

  // Merges {other} into the current space. Note that this modifies {other},
  // e.g., removes its bump pointer area and resets statistics.
  void MergeLocalSpace(LocalSpace* other);

  // Refills the free list from the corresponding free list filled by the
  // sweeper.
  virtual void RefillFreeList();

  base::Mutex* mutex() { return &space_mutex_; }

  inline void UnlinkFreeListCategories(Page* page);
  inline size_t RelinkFreeListCategories(Page* page);

  Page* first_page() { return reinterpret_cast<Page*>(Space::first_page()); }
  const Page* first_page() const {
    return reinterpret_cast<const Page*>(Space::first_page());
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

  void SetLinearAllocationArea(Address top, Address limit);

  Address original_top_acquire() {
    return original_top_.load(std::memory_order_acquire);
  }

  Address original_limit_relaxed() {
    return original_limit_.load(std::memory_order_relaxed);
  }

  void MoveOriginalTopForward() {
    DCHECK_GE(top(), original_top_);
    DCHECK_LE(top(), original_limit_);
    original_top_.store(top(), std::memory_order_release);
  }

 private:
  class ConcurrentAllocationMutex {
   public:
    explicit ConcurrentAllocationMutex(PagedSpace* space) {
      if (space->SupportsConcurrentAllocation()) {
        guard_.emplace(&space->space_mutex_);
      }
    }

    base::Optional<base::MutexGuard> guard_;
  };

  bool SupportsConcurrentAllocation() { return !is_local_space(); }

  // Set space linear allocation area.
  void SetTopAndLimit(Address top, Address limit);
  void DecreaseLimit(Address new_limit);
  void UpdateInlineAllocationLimit(size_t min_size) override;
  bool SupportsAllocationObserver() override { return !is_local_space(); }

  // Slow path of allocation function
  V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRawSlow(int size_in_bytes, AllocationAlignment alignment,
                  AllocationOrigin origin);

 protected:
  // PagedSpaces that should be included in snapshots have different, i.e.,
  // smaller, initial pages.
  virtual bool snapshotable() { return true; }

  bool HasPages() { return first_page() != nullptr; }

  // Cleans up the space, frees all pages in this space except those belonging
  // to the initial chunk, uncommits addresses in the initial chunk.
  void TearDown();

  // Expands the space by allocating a fixed number of pages. Returns false if
  // it cannot allocate requested number of pages from OS, or if the hard heap
  // size limit has been hit.
  virtual Page* Expand();
  Page* ExpandBackground(LocalHeap* local_heap);
  Page* AllocatePage();

  // Sets up a linear allocation area that fits the given number of bytes.
  // Returns false if there is not enough space and the caller has to retry
  // after collecting garbage.
  inline bool EnsureLabMain(int size_in_bytes, AllocationOrigin origin);
  // Allocates an object from the linear allocation area. Assumes that the
  // linear allocation area is large enought to fit the object.
  inline AllocationResult AllocateFastUnaligned(int size_in_bytes);
  // Tries to allocate an aligned object from the linear allocation area.
  // Returns nullptr if the linear allocation area does not fit the object.
  // Otherwise, returns the object pointer and writes the allocation size
  // (object size + alignment filler size) to the size_in_bytes.
  inline AllocationResult AllocateFastAligned(int size_in_bytes,
                                              int* aligned_size_in_bytes,
                                              AllocationAlignment alignment);

  V8_WARN_UNUSED_RESULT bool TryAllocationFromFreeListMain(
      size_t size_in_bytes, AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT bool ContributeToSweepingMain(int required_freed_bytes,
                                                      int max_pages,
                                                      int size_in_bytes,
                                                      AllocationOrigin origin);

  // Refills LAB for EnsureLabMain. This function is space-dependent. Returns
  // false if there is not enough space and the caller has to retry after
  // collecting garbage.
  V8_WARN_UNUSED_RESULT virtual bool RefillLabMain(int size_in_bytes,
                                                   AllocationOrigin origin);

  // Actual implementation of refilling LAB. Returns false if there is not
  // enough space and the caller has to retry after collecting garbage.
  V8_WARN_UNUSED_RESULT bool RawRefillLabMain(int size_in_bytes,
                                              AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT base::Optional<std::pair<Address, size_t>>
  TryAllocationFromFreeListBackground(LocalHeap* local_heap,
                                      size_t min_size_in_bytes,
                                      size_t max_size_in_bytes,
                                      AllocationAlignment alignment,
                                      AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT bool TryExpand(int size_in_bytes,
                                       AllocationOrigin origin);

  Executability executable_;

  LocalSpaceKind local_space_kind_;

  size_t area_size_;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  // Mutex guarding any concurrent access to the space.
  base::Mutex space_mutex_;

  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks.
  std::atomic<Address> original_top_;
  std::atomic<Address> original_limit_;

  friend class IncrementalMarking;
  friend class MarkCompactCollector;

  // Used in cctest.
  friend class heap::HeapTester;
};

// -----------------------------------------------------------------------------
// Base class for compaction space and off-thread space.

class V8_EXPORT_PRIVATE LocalSpace : public PagedSpace {
 public:
  LocalSpace(Heap* heap, AllocationSpace id, Executability executable,
             LocalSpaceKind local_space_kind)
      : PagedSpace(heap, id, executable, FreeList::CreateFreeList(),
                   local_space_kind) {
    DCHECK_NE(local_space_kind, LocalSpaceKind::kNone);
  }

  const std::vector<Page*>& GetNewPages() { return new_pages_; }

 protected:
  Page* Expand() override;
  // The space is temporary and not included in any snapshots.
  bool snapshotable() override { return false; }
  // Pages that were allocated in this local space and need to be merged
  // to the main space.
  std::vector<Page*> new_pages_;
};

// -----------------------------------------------------------------------------
// Compaction space that is used temporarily during compaction.

class V8_EXPORT_PRIVATE CompactionSpace : public LocalSpace {
 public:
  CompactionSpace(Heap* heap, AllocationSpace id, Executability executable,
                  LocalSpaceKind local_space_kind)
      : LocalSpace(heap, id, executable, local_space_kind) {
    DCHECK(is_compaction_space());
  }

 protected:
  V8_WARN_UNUSED_RESULT bool RefillLabMain(int size_in_bytes,
                                           AllocationOrigin origin) override;
};

// A collection of |CompactionSpace|s used by a single compaction task.
class CompactionSpaceCollection : public Malloced {
 public:
  explicit CompactionSpaceCollection(Heap* heap,
                                     LocalSpaceKind local_space_kind)
      : old_space_(heap, OLD_SPACE, Executability::NOT_EXECUTABLE,
                   local_space_kind),
        code_space_(heap, CODE_SPACE, Executability::EXECUTABLE,
                    local_space_kind) {}

  CompactionSpace* Get(AllocationSpace space) {
    switch (space) {
      case OLD_SPACE:
        return &old_space_;
      case CODE_SPACE:
        return &code_space_;
      default:
        UNREACHABLE();
    }
    UNREACHABLE();
  }

 private:
  CompactionSpace old_space_;
  CompactionSpace code_space_;
};

// -----------------------------------------------------------------------------
// Old generation regular object space.

class OldSpace : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  explicit OldSpace(Heap* heap)
      : PagedSpace(heap, OLD_SPACE, NOT_EXECUTABLE,
                   FreeList::CreateFreeList()) {}

  static bool IsAtPageStart(Address addr) {
    return static_cast<intptr_t>(addr & kPageAlignmentMask) ==
           MemoryChunkLayout::ObjectStartOffsetInDataPage();
  }

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) const final {
    if (type == ExternalBackingStoreType::kArrayBuffer)
      return heap()->OldArrayBufferBytes();
    return external_backing_store_bytes_[type];
  }
};

// -----------------------------------------------------------------------------
// Old generation code object space.

class CodeSpace : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  explicit CodeSpace(Heap* heap)
      : PagedSpace(heap, CODE_SPACE, EXECUTABLE, FreeList::CreateFreeList()) {}
};

// -----------------------------------------------------------------------------
// Old space for all map objects

class MapSpace : public PagedSpace {
 public:
  // Creates a map space object.
  explicit MapSpace(Heap* heap)
      : PagedSpace(heap, MAP_SPACE, NOT_EXECUTABLE,
                   FreeList::CreateFreeList()) {}

  int RoundSizeDownToObjectAlignment(int size) override {
    if (base::bits::IsPowerOfTwo(Map::kSize)) {
      return RoundDown(size, Map::kSize);
    } else {
      return (size / Map::kSize) * Map::kSize;
    }
  }

  void SortFreeList();

#ifdef VERIFY_HEAP
  void VerifyObject(HeapObject obj) override;
#endif
};

// Iterates over the chunks (pages and large object pages) that can contain
// pointers to new space or to evacuation candidates.
class OldGenerationMemoryChunkIterator {
 public:
  inline explicit OldGenerationMemoryChunkIterator(Heap* heap);

  // Return nullptr when the iterator is done.
  inline MemoryChunk* next();

 private:
  enum State {
    kOldSpaceState,
    kMapState,
    kCodeState,
    kLargeObjectState,
    kCodeLargeObjectState,
    kFinishedState
  };
  Heap* heap_;
  State state_;
  PageIterator old_iterator_;
  PageIterator code_iterator_;
  PageIterator map_iterator_;
  LargePageIterator lo_iterator_;
  LargePageIterator code_lo_iterator_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGED_SPACES_H_
