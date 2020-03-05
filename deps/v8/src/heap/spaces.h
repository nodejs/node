// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SPACES_H_
#define V8_HEAP_SPACES_H_

#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/base/atomic-utils.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/export-template.h"
#include "src/base/iterator.h"
#include "src/base/list.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap.h"
#include "src/heap/invalidated-slots.h"
#include "src/heap/marking.h"
#include "src/heap/slot-set.h"
#include "src/objects/free-space.h"
#include "src/objects/heap-object.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/tasks/cancelable-task.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

namespace heap {
class HeapTester;
class TestCodePageAllocatorScope;
}  // namespace heap

class AllocationObserver;
class CompactionSpace;
class CompactionSpaceCollection;
class FreeList;
class Isolate;
class LargeObjectSpace;
class LinearAllocationArea;
class LocalArrayBufferTracker;
class LocalSpace;
class MemoryAllocator;
class MemoryChunk;
class MemoryChunkLayout;
class OffThreadSpace;
class Page;
class PagedSpace;
class SemiSpace;
class SlotsBuffer;
class SlotSet;
class TypedSlotSet;
class Space;

// -----------------------------------------------------------------------------
// Heap structures:
//
// A JS heap consists of a young generation, an old generation, and a large
// object space. The young generation is divided into two semispaces. A
// scavenger implements Cheney's copying algorithm. The old generation is
// separated into a map space and an old object space. The map space contains
// all (and only) map objects, the rest of old objects go into the old space.
// The old generation is collected by a mark-sweep-compact collector.
//
// The semispaces of the young generation are contiguous.  The old and map
// spaces consists of a list of pages. A page has a page header and an object
// area.
//
// There is a separate large object space for objects larger than
// kMaxRegularHeapObjectSize, so that they do not have to move during
// collection. The large object space is paged. Pages in large object space
// may be larger than the page size.
//
// A store-buffer based write barrier is used to keep track of intergenerational
// references.  See heap/store-buffer.h.
//
// During scavenges and mark-sweep collections we sometimes (after a store
// buffer overflow) iterate intergenerational pointers without decoding heap
// object maps so if the page belongs to old space or large object space
// it is essential to guarantee that the page does not contain any
// garbage pointers to new space: every pointer aligned word which satisfies
// the Heap::InNewSpace() predicate must be a pointer to a live heap object in
// new space. Thus objects in old space and large object spaces should have a
// special layout (e.g. no bare integer fields). This requirement does not
// apply to map space which is iterated in a special fashion. However we still
// require pointer fields of dead maps to be cleaned.
//
// To enable lazy cleaning of old space pages we can mark chunks of the page
// as being garbage.  Garbage sections are marked with a special map.  These
// sections are skipped when scanning the page, even if we are otherwise
// scanning without regard for object boundaries.  Garbage sections are chained
// together to form a free list after a GC.  Garbage sections created outside
// of GCs by object trunctation etc. may not be in the free list chain.  Very
// small free spaces are ignored, they need only be cleaned of bogus pointers
// into new space.
//
// Each page may have up to one special garbage section.  The start of this
// section is denoted by the top field in the space.  The end of the section
// is denoted by the limit field in the space.  This special garbage section
// is not marked with a free space map in the data.  The point of this section
// is to enable linear allocation without having to constantly update the byte
// array every time the top field is updated and a new object is created.  The
// special garbage section is not in the chain of garbage sections.
//
// Since the top and limit fields are in the space, not the page, only one page
// has a special garbage section, and if the top and limit are equal then there
// is no special garbage section.

// Some assertion macros used in the debugging mode.

#define DCHECK_OBJECT_SIZE(size) \
  DCHECK((0 < size) && (size <= kMaxRegularHeapObjectSize))

#define DCHECK_CODEOBJECT_SIZE(size, code_space) \
  DCHECK((0 < size) && (size <= code_space->AreaSize()))

using FreeListCategoryType = int32_t;

static const FreeListCategoryType kFirstCategory = 0;
static const FreeListCategoryType kInvalidCategory = -1;

enum FreeMode { kLinkCategory, kDoNotLinkCategory };

enum class SpaceAccountingMode { kSpaceAccounted, kSpaceUnaccounted };

// A free list category maintains a linked list of free memory blocks.
class FreeListCategory {
 public:
  void Initialize(FreeListCategoryType type) {
    type_ = type;
    available_ = 0;
    prev_ = nullptr;
    next_ = nullptr;
  }

  void Reset(FreeList* owner);

  void RepairFreeList(Heap* heap);

  // Relinks the category into the currently owning free list. Requires that the
  // category is currently unlinked.
  void Relink(FreeList* owner);

  void Free(Address address, size_t size_in_bytes, FreeMode mode,
            FreeList* owner);

  // Performs a single try to pick a node of at least |minimum_size| from the
  // category. Stores the actual size in |node_size|. Returns nullptr if no
  // node is found.
  FreeSpace PickNodeFromList(size_t minimum_size, size_t* node_size);

  // Picks a node of at least |minimum_size| from the category. Stores the
  // actual size in |node_size|. Returns nullptr if no node is found.
  FreeSpace SearchForNodeInList(size_t minimum_size, size_t* node_size);

  inline bool is_linked(FreeList* owner) const;
  bool is_empty() { return top().is_null(); }
  uint32_t available() const { return available_; }

  size_t SumFreeList();
  int FreeListLength();

 private:
  // For debug builds we accurately compute free lists lengths up until
  // {kVeryLongFreeList} by manually walking the list.
  static const int kVeryLongFreeList = 500;

  // Updates |available_|, |length_| and free_list_->Available() after an
  // allocation of size |allocation_size|.
  inline void UpdateCountersAfterAllocation(size_t allocation_size);

  FreeSpace top() { return top_; }
  void set_top(FreeSpace top) { top_ = top; }
  FreeListCategory* prev() { return prev_; }
  void set_prev(FreeListCategory* prev) { prev_ = prev; }
  FreeListCategory* next() { return next_; }
  void set_next(FreeListCategory* next) { next_ = next; }

  // |type_|: The type of this free list category.
  FreeListCategoryType type_ = kInvalidCategory;

  // |available_|: Total available bytes in all blocks of this free list
  // category.
  uint32_t available_ = 0;

  // |top_|: Points to the top FreeSpace in the free list category.
  FreeSpace top_;

  FreeListCategory* prev_ = nullptr;
  FreeListCategory* next_ = nullptr;

  friend class FreeList;
  friend class FreeListManyCached;
  friend class PagedSpace;
  friend class MapSpace;
};

// A free list maintains free blocks of memory. The free list is organized in
// a way to encourage objects allocated around the same time to be near each
// other. The normal way to allocate is intended to be by bumping a 'top'
// pointer until it hits a 'limit' pointer.  When the limit is hit we need to
// find a new space to allocate from. This is done with the free list, which is
// divided up into rough categories to cut down on waste. Having finer
// categories would scatter allocation more.
class FreeList {
 public:
  // Creates a Freelist of the default class (FreeListLegacy for now).
  V8_EXPORT_PRIVATE static FreeList* CreateFreeList();

  virtual ~FreeList() = default;

  // Returns how much memory can be allocated after freeing maximum_freed
  // memory.
  virtual size_t GuaranteedAllocatable(size_t maximum_freed) = 0;

  // Adds a node on the free list. The block of size {size_in_bytes} starting
  // at {start} is placed on the free list. The return value is the number of
  // bytes that were not added to the free list, because the freed memory block
  // was too small. Bookkeeping information will be written to the block, i.e.,
  // its contents will be destroyed. The start address should be word aligned,
  // and the size should be a non-zero multiple of the word size.
  virtual size_t Free(Address start, size_t size_in_bytes, FreeMode mode);

  // Allocates a free space node frome the free list of at least size_in_bytes
  // bytes. Returns the actual node size in node_size which can be bigger than
  // size_in_bytes. This method returns null if the allocation request cannot be
  // handled by the free list.
  virtual V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                                   size_t* node_size,
                                                   AllocationOrigin origin) = 0;

  // Returns a page containing an entry for a given type, or nullptr otherwise.
  V8_EXPORT_PRIVATE virtual Page* GetPageForSize(size_t size_in_bytes) = 0;

  virtual void Reset();

  // Return the number of bytes available on the free list.
  size_t Available() {
    DCHECK(available_ == SumFreeLists());
    return available_;
  }

  // Update number of available  bytes on the Freelists.
  void IncreaseAvailableBytes(size_t bytes) { available_ += bytes; }
  void DecreaseAvailableBytes(size_t bytes) { available_ -= bytes; }

  bool IsEmpty() {
    bool empty = true;
    ForAllFreeListCategories([&empty](FreeListCategory* category) {
      if (!category->is_empty()) empty = false;
    });
    return empty;
  }

  // Used after booting the VM.
  void RepairLists(Heap* heap);

  V8_EXPORT_PRIVATE size_t EvictFreeListItems(Page* page);

  int number_of_categories() { return number_of_categories_; }
  FreeListCategoryType last_category() { return last_category_; }

  size_t wasted_bytes() { return wasted_bytes_; }

  template <typename Callback>
  void ForAllFreeListCategories(FreeListCategoryType type, Callback callback) {
    FreeListCategory* current = categories_[type];
    while (current != nullptr) {
      FreeListCategory* next = current->next();
      callback(current);
      current = next;
    }
  }

  template <typename Callback>
  void ForAllFreeListCategories(Callback callback) {
    for (int i = kFirstCategory; i < number_of_categories(); i++) {
      ForAllFreeListCategories(static_cast<FreeListCategoryType>(i), callback);
    }
  }

  virtual bool AddCategory(FreeListCategory* category);
  virtual V8_EXPORT_PRIVATE void RemoveCategory(FreeListCategory* category);
  void PrintCategories(FreeListCategoryType type);

 protected:
  class FreeListCategoryIterator final {
   public:
    FreeListCategoryIterator(FreeList* free_list, FreeListCategoryType type)
        : current_(free_list->categories_[type]) {}

    bool HasNext() const { return current_ != nullptr; }

    FreeListCategory* Next() {
      DCHECK(HasNext());
      FreeListCategory* tmp = current_;
      current_ = current_->next();
      return tmp;
    }

   private:
    FreeListCategory* current_;
  };

#ifdef DEBUG
  V8_EXPORT_PRIVATE size_t SumFreeLists();
  bool IsVeryLong();
#endif

  // Tries to retrieve a node from the first category in a given |type|.
  // Returns nullptr if the category is empty or the top entry is smaller
  // than minimum_size.
  FreeSpace TryFindNodeIn(FreeListCategoryType type, size_t minimum_size,
                          size_t* node_size);

  // Searches a given |type| for a node of at least |minimum_size|.
  FreeSpace SearchForNodeInList(FreeListCategoryType type, size_t minimum_size,
                                size_t* node_size);

  // Returns the smallest category in which an object of |size_in_bytes| could
  // fit.
  virtual FreeListCategoryType SelectFreeListCategoryType(
      size_t size_in_bytes) = 0;

  FreeListCategory* top(FreeListCategoryType type) const {
    return categories_[type];
  }

  inline Page* GetPageForCategoryType(FreeListCategoryType type);

  int number_of_categories_ = 0;
  FreeListCategoryType last_category_ = 0;
  size_t min_block_size_ = 0;

  std::atomic<size_t> wasted_bytes_{0};
  FreeListCategory** categories_ = nullptr;

  // |available_|: The number of bytes in this freelist.
  size_t available_ = 0;

  friend class FreeListCategory;
  friend class Page;
  friend class MemoryChunk;
  friend class ReadOnlyPage;
  friend class MapSpace;
};

// FreeList used for spaces that don't have freelists
// (only the LargeObject space for now).
class NoFreeList final : public FreeList {
 public:
  size_t GuaranteedAllocatable(size_t maximum_freed) final {
    FATAL("NoFreeList can't be used as a standard FreeList. ");
  }
  size_t Free(Address start, size_t size_in_bytes, FreeMode mode) final {
    FATAL("NoFreeList can't be used as a standard FreeList.");
  }
  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) final {
    FATAL("NoFreeList can't be used as a standard FreeList.");
  }
  Page* GetPageForSize(size_t size_in_bytes) final {
    FATAL("NoFreeList can't be used as a standard FreeList.");
  }

 private:
  FreeListCategoryType SelectFreeListCategoryType(size_t size_in_bytes) final {
    FATAL("NoFreeList can't be used as a standard FreeList.");
  }
};

// ----------------------------------------------------------------------------
// Space is the abstract superclass for all allocation spaces.
class V8_EXPORT_PRIVATE Space : public Malloced {
 public:
  Space(Heap* heap, AllocationSpace id, FreeList* free_list)
      : allocation_observers_paused_(false),
        heap_(heap),
        id_(id),
        committed_(0),
        max_committed_(0),
        free_list_(std::unique_ptr<FreeList>(free_list)) {
    external_backing_store_bytes_ =
        new std::atomic<size_t>[ExternalBackingStoreType::kNumTypes];
    external_backing_store_bytes_[ExternalBackingStoreType::kArrayBuffer] = 0;
    external_backing_store_bytes_[ExternalBackingStoreType::kExternalString] =
        0;
  }

  static inline void MoveExternalBackingStoreBytes(
      ExternalBackingStoreType type, Space* from, Space* to, size_t amount);

  virtual ~Space() {
    delete[] external_backing_store_bytes_;
    external_backing_store_bytes_ = nullptr;
  }

  Heap* heap() const {
    DCHECK_NOT_NULL(heap_);
    return heap_;
  }

  bool IsDetached() const { return heap_ == nullptr; }

  AllocationSpace identity() { return id_; }

  const char* name() { return Heap::GetSpaceName(id_); }

  virtual void AddAllocationObserver(AllocationObserver* observer);

  virtual void RemoveAllocationObserver(AllocationObserver* observer);

  virtual void PauseAllocationObservers();

  virtual void ResumeAllocationObservers();

  virtual void StartNextInlineAllocationStep() {}

  void AllocationStep(int bytes_since_last, Address soon_object, int size);

  // An AllocationStep equivalent to be called after merging a contiguous
  // chunk of an off-thread space into this space. The chunk is treated as a
  // single allocation-folding group.
  void AllocationStepAfterMerge(Address first_object_in_chunk, int size);

  // Return the total amount committed memory for this space, i.e., allocatable
  // memory and page headers.
  virtual size_t CommittedMemory() { return committed_; }

  virtual size_t MaximumCommittedMemory() { return max_committed_; }

  // Returns allocated size.
  virtual size_t Size() = 0;

  // Returns size of objects. Can differ from the allocated size
  // (e.g. see OldLargeObjectSpace).
  virtual size_t SizeOfObjects() { return Size(); }

  // Approximate amount of physical memory committed for this space.
  virtual size_t CommittedPhysicalMemory() = 0;

  // Return the available bytes without growing.
  virtual size_t Available() = 0;

  virtual int RoundSizeDownToObjectAlignment(int size) {
    if (id_ == CODE_SPACE) {
      return RoundDown(size, kCodeAlignment);
    } else {
      return RoundDown(size, kTaggedSize);
    }
  }

  virtual std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) = 0;

  void AccountCommitted(size_t bytes) {
    DCHECK_GE(committed_ + bytes, committed_);
    committed_ += bytes;
    if (committed_ > max_committed_) {
      max_committed_ = committed_;
    }
  }

  void AccountUncommitted(size_t bytes) {
    DCHECK_GE(committed_, committed_ - bytes);
    committed_ -= bytes;
  }

  inline void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  inline void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  // Returns amount of off-heap memory in-use by objects in this Space.
  virtual size_t ExternalBackingStoreBytes(
      ExternalBackingStoreType type) const {
    return external_backing_store_bytes_[type];
  }

  void* GetRandomMmapAddr();

  MemoryChunk* first_page() { return memory_chunk_list_.front(); }
  MemoryChunk* last_page() { return memory_chunk_list_.back(); }

  base::List<MemoryChunk>& memory_chunk_list() { return memory_chunk_list_; }

  FreeList* free_list() { return free_list_.get(); }

#ifdef DEBUG
  virtual void Print() = 0;
#endif

 protected:
  intptr_t GetNextInlineAllocationStepSize();
  bool AllocationObserversActive() {
    return !allocation_observers_paused_ && !allocation_observers_.empty();
  }

  void DetachFromHeap() { heap_ = nullptr; }

  std::vector<AllocationObserver*> allocation_observers_;

  // The List manages the pages that belong to the given space.
  base::List<MemoryChunk> memory_chunk_list_;

  // Tracks off-heap memory used by this space.
  std::atomic<size_t>* external_backing_store_bytes_;

  bool allocation_observers_paused_;
  Heap* heap_;
  AllocationSpace id_;

  // Keeps track of committed memory in a space.
  size_t committed_;
  size_t max_committed_;

  std::unique_ptr<FreeList> free_list_;

  DISALLOW_COPY_AND_ASSIGN(Space);
};

// The CodeObjectRegistry holds all start addresses of code objects of a given
// MemoryChunk. Each MemoryChunk owns a separate CodeObjectRegistry. The
// CodeObjectRegistry allows fast lookup from an inner pointer of a code object
// to the actual code object.
class V8_EXPORT_PRIVATE CodeObjectRegistry {
 public:
  void RegisterNewlyAllocatedCodeObject(Address code);
  void RegisterAlreadyExistingCodeObject(Address code);
  void Clear();
  void Finalize();
  bool Contains(Address code) const;
  Address GetCodeObjectStartFromInnerAddress(Address address) const;

 private:
  std::vector<Address> code_object_registry_already_existing_;
  std::set<Address> code_object_registry_newly_allocated_;
};

class V8_EXPORT_PRIVATE MemoryChunkLayout {
 public:
  static size_t CodePageGuardStartOffset();
  static size_t CodePageGuardSize();
  static intptr_t ObjectStartOffsetInCodePage();
  static intptr_t ObjectEndOffsetInCodePage();
  static size_t AllocatableMemoryInCodePage();
  static intptr_t ObjectStartOffsetInDataPage();
  static size_t AllocatableMemoryInDataPage();
  static size_t ObjectStartOffsetInMemoryChunk(AllocationSpace space);
  static size_t AllocatableMemoryInMemoryChunk(AllocationSpace space);
};

// MemoryChunk represents a memory region owned by a specific space.
// It is divided into the header and the body. Chunk start is always
// 1MB aligned. Start of the body is aligned so it can accommodate
// any heap object.
class MemoryChunk : public BasicMemoryChunk {
 public:
  // Use with std data structures.
  struct Hasher {
    size_t operator()(MemoryChunk* const chunk) const {
      return reinterpret_cast<size_t>(chunk) >> kPageSizeBits;
    }
  };

  using Flags = uintptr_t;

  static const Flags kPointersToHereAreInterestingMask =
      POINTERS_TO_HERE_ARE_INTERESTING;

  static const Flags kPointersFromHereAreInterestingMask =
      POINTERS_FROM_HERE_ARE_INTERESTING;

  static const Flags kEvacuationCandidateMask = EVACUATION_CANDIDATE;

  static const Flags kIsInYoungGenerationMask = FROM_PAGE | TO_PAGE;

  static const Flags kIsLargePageMask = LARGE_PAGE;

  static const Flags kSkipEvacuationSlotsRecordingMask =
      kEvacuationCandidateMask | kIsInYoungGenerationMask;

  // |kDone|: The page state when sweeping is complete or sweeping must not be
  //   performed on that page. Sweeper threads that are done with their work
  //   will set this value and not touch the page anymore.
  // |kPending|: This page is ready for parallel sweeping.
  // |kInProgress|: This page is currently swept by a sweeper thread.
  enum class ConcurrentSweepingState : intptr_t {
    kDone,
    kPending,
    kInProgress,
  };

  static const size_t kHeaderSize =
      BasicMemoryChunk::kHeaderSize  // Parent size.
      + 3 * kSystemPointerSize       // VirtualMemory reservation_
      + kSystemPointerSize           // Address owner_
      + kSizetSize                   // size_t progress_bar_
      + kIntptrSize                  // intptr_t live_byte_count_
      + kSystemPointerSize           // SlotSet* sweeping_slot_set_
      + kSystemPointerSize *
            NUMBER_OF_REMEMBERED_SET_TYPES  // TypedSlotSet* array
      + kSystemPointerSize *
            NUMBER_OF_REMEMBERED_SET_TYPES  // InvalidatedSlots* array
      + kSystemPointerSize  // std::atomic<intptr_t> high_water_mark_
      + kSystemPointerSize  // base::Mutex* mutex_
      + kSystemPointerSize  // std::atomic<ConcurrentSweepingState>
                            // concurrent_sweeping_
      + kSystemPointerSize  // base::Mutex* page_protection_change_mutex_
      + kSystemPointerSize  // unitptr_t write_unprotect_counter_
      + kSizetSize * ExternalBackingStoreType::kNumTypes
      // std::atomic<size_t> external_backing_store_bytes_
      + kSizetSize              // size_t allocated_bytes_
      + kSizetSize              // size_t wasted_memory_
      + kSystemPointerSize * 2  // base::ListNode
      + kSystemPointerSize      // FreeListCategory** categories__
      + kSystemPointerSize      // LocalArrayBufferTracker* local_tracker_
      + kIntptrSize  // std::atomic<intptr_t> young_generation_live_byte_count_
      + kSystemPointerSize   // Bitmap* young_generation_bitmap_
      + kSystemPointerSize   // CodeObjectRegistry* code_object_registry_
      + kSystemPointerSize;  // PossiblyEmptyBuckets possibly_empty_buckets_

  // Page size in bytes.  This must be a multiple of the OS page size.
  static const int kPageSize = 1 << kPageSizeBits;

  // Maximum number of nested code memory modification scopes.
  static const int kMaxWriteUnprotectCounter = 3;

  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromAddress(Address a) {
    return reinterpret_cast<MemoryChunk*>(BaseAddress(a));
  }
  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromHeapObject(HeapObject o) {
    return reinterpret_cast<MemoryChunk*>(BaseAddress(o.ptr()));
  }

  void SetOldGenerationPageFlags(bool is_marking);
  void SetYoungGenerationPageFlags(bool is_marking);

  static inline void UpdateHighWaterMark(Address mark) {
    if (mark == kNullAddress) return;
    // Need to subtract one from the mark because when a chunk is full the
    // top points to the next address after the chunk, which effectively belongs
    // to another chunk. See the comment to Page::FromAllocationAreaAddress.
    MemoryChunk* chunk = MemoryChunk::FromAddress(mark - 1);
    intptr_t new_mark = static_cast<intptr_t>(mark - chunk->address());
    intptr_t old_mark = 0;
    do {
      old_mark = chunk->high_water_mark_;
    } while (
        (new_mark > old_mark) &&
        !chunk->high_water_mark_.compare_exchange_weak(old_mark, new_mark));
  }

  static inline void MoveExternalBackingStoreBytes(
      ExternalBackingStoreType type, MemoryChunk* from, MemoryChunk* to,
      size_t amount);

  void DiscardUnusedMemory(Address addr, size_t size);

  base::Mutex* mutex() { return mutex_; }

  void set_concurrent_sweeping_state(ConcurrentSweepingState state) {
    concurrent_sweeping_ = state;
  }

  ConcurrentSweepingState concurrent_sweeping_state() {
    return static_cast<ConcurrentSweepingState>(concurrent_sweeping_.load());
  }

  bool SweepingDone() {
    return concurrent_sweeping_ == ConcurrentSweepingState::kDone;
  }

  inline Heap* heap() const {
    DCHECK_NOT_NULL(heap_);
    return heap_;
  }

#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race in
  // mark-bit initialization. See MemoryChunk::Initialize for the corresponding
  // release store.
  void SynchronizedHeapLoad();
#endif

  template <RememberedSetType type>
  bool ContainsSlots() {
    return slot_set<type>() != nullptr || typed_slot_set<type>() != nullptr ||
           invalidated_slots<type>() != nullptr;
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  SlotSet* slot_set() {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&slot_set_[type]);
    return slot_set_[type];
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  SlotSet* sweeping_slot_set() {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&sweeping_slot_set_);
    return sweeping_slot_set_;
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  TypedSlotSet* typed_slot_set() {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&typed_slot_set_[type]);
    return typed_slot_set_[type];
  }

  template <RememberedSetType type>
  V8_EXPORT_PRIVATE SlotSet* AllocateSlotSet();
  SlotSet* AllocateSweepingSlotSet();
  SlotSet* AllocateSlotSet(SlotSet** slot_set);

  // Not safe to be called concurrently.
  template <RememberedSetType type>
  void ReleaseSlotSet();
  void ReleaseSlotSet(SlotSet** slot_set);
  void ReleaseSweepingSlotSet();
  template <RememberedSetType type>
  TypedSlotSet* AllocateTypedSlotSet();
  // Not safe to be called concurrently.
  template <RememberedSetType type>
  void ReleaseTypedSlotSet();

  template <RememberedSetType type>
  InvalidatedSlots* AllocateInvalidatedSlots();
  template <RememberedSetType type>
  void ReleaseInvalidatedSlots();
  template <RememberedSetType type>
  V8_EXPORT_PRIVATE void RegisterObjectWithInvalidatedSlots(HeapObject object);
  void InvalidateRecordedSlots(HeapObject object);
  template <RememberedSetType type>
  bool RegisteredObjectWithInvalidatedSlots(HeapObject object);
  template <RememberedSetType type>
  InvalidatedSlots* invalidated_slots() {
    return invalidated_slots_[type];
  }

  void ReleaseLocalTracker();

  void AllocateYoungGenerationBitmap();
  void ReleaseYoungGenerationBitmap();

  int FreeListsLength();

  // Approximate amount of physical memory committed for this chunk.
  V8_EXPORT_PRIVATE size_t CommittedPhysicalMemory();

  Address HighWaterMark() { return address() + high_water_mark_; }

  size_t ProgressBar() {
    DCHECK(IsFlagSet<AccessMode::ATOMIC>(HAS_PROGRESS_BAR));
    return progress_bar_.load(std::memory_order_acquire);
  }

  bool TrySetProgressBar(size_t old_value, size_t new_value) {
    DCHECK(IsFlagSet<AccessMode::ATOMIC>(HAS_PROGRESS_BAR));
    return progress_bar_.compare_exchange_strong(old_value, new_value,
                                                 std::memory_order_acq_rel);
  }

  void ResetProgressBar() {
    if (IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
      progress_bar_.store(0, std::memory_order_release);
    }
  }

  inline void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  inline void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) {
    return external_backing_store_bytes_[type];
  }

  // Some callers rely on the fact that this can operate on both
  // tagged and aligned object addresses.
  inline uint32_t AddressToMarkbitIndex(Address addr) const {
    return static_cast<uint32_t>(addr - this->address()) >> kTaggedSizeLog2;
  }

  inline Address MarkbitIndexToAddress(uint32_t index) const {
    return this->address() + (index << kTaggedSizeLog2);
  }

  bool NeverEvacuate() { return IsFlagSet(NEVER_EVACUATE); }

  void MarkNeverEvacuate() { SetFlag(NEVER_EVACUATE); }

  bool CanAllocate() {
    return !IsEvacuationCandidate() && !IsFlagSet(NEVER_ALLOCATE_ON_PAGE);
  }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  bool IsEvacuationCandidate() {
    DCHECK(!(IsFlagSet<access_mode>(NEVER_EVACUATE) &&
             IsFlagSet<access_mode>(EVACUATION_CANDIDATE)));
    return IsFlagSet<access_mode>(EVACUATION_CANDIDATE);
  }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  bool ShouldSkipEvacuationSlotRecording() {
    uintptr_t flags = GetFlags<access_mode>();
    return ((flags & kSkipEvacuationSlotsRecordingMask) != 0) &&
           ((flags & COMPACTION_WAS_ABORTED) == 0);
  }

  Executability executable() {
    return IsFlagSet(IS_EXECUTABLE) ? EXECUTABLE : NOT_EXECUTABLE;
  }

  bool IsFromPage() const { return IsFlagSet(FROM_PAGE); }
  bool IsToPage() const { return IsFlagSet(TO_PAGE); }
  bool IsLargePage() const { return IsFlagSet(LARGE_PAGE); }
  bool InYoungGeneration() const {
    return (GetFlags() & kIsInYoungGenerationMask) != 0;
  }
  bool InNewSpace() const { return InYoungGeneration() && !IsLargePage(); }
  bool InNewLargeObjectSpace() const {
    return InYoungGeneration() && IsLargePage();
  }
  bool InOldSpace() const;
  V8_EXPORT_PRIVATE bool InLargeObjectSpace() const;

  // Gets the chunk's owner or null if the space has been detached.
  Space* owner() const { return owner_; }

  void set_owner(Space* space) { owner_ = space; }

  bool IsWritable() const {
    // If this is a read-only space chunk but heap_ is non-null, it has not yet
    // been sealed and can be written to.
    return !InReadOnlySpace() || heap_ != nullptr;
  }

  // Gets the chunk's allocation space, potentially dealing with a null owner_
  // (like read-only chunks have).
  inline AllocationSpace owner_identity() const;

  // Emits a memory barrier. For TSAN builds the other thread needs to perform
  // MemoryChunk::synchronized_heap() to simulate the barrier.
  void InitializationMemoryFence();

  V8_EXPORT_PRIVATE void SetReadable();
  V8_EXPORT_PRIVATE void SetReadAndExecutable();
  V8_EXPORT_PRIVATE void SetReadAndWritable();

  void SetDefaultCodePermissions() {
    if (FLAG_jitless) {
      SetReadable();
    } else {
      SetReadAndExecutable();
    }
  }

  base::ListNode<MemoryChunk>& list_node() { return list_node_; }

  CodeObjectRegistry* GetCodeObjectRegistry() { return code_object_registry_; }

  FreeList* free_list() { return owner()->free_list(); }

  PossiblyEmptyBuckets* possibly_empty_buckets() {
    return &possibly_empty_buckets_;
  }

 protected:
  static MemoryChunk* Initialize(Heap* heap, Address base, size_t size,
                                 Address area_start, Address area_end,
                                 Executability executable, Space* owner,
                                 VirtualMemory reservation);

  // Release all memory allocated by the chunk. Should be called when memory
  // chunk is about to be freed.
  void ReleaseAllAllocatedMemory();
  // Release memory allocated by the chunk, except that which is needed by
  // read-only space chunks.
  void ReleaseAllocatedMemoryNeededForWritableChunk();

  // Sets the requested page permissions only if the write unprotect counter
  // has reached 0.
  void DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::Permission permission);

  VirtualMemory* reserved_memory() { return &reservation_; }

  template <AccessMode mode>
  ConcurrentBitmap<mode>* marking_bitmap() const {
    return reinterpret_cast<ConcurrentBitmap<mode>*>(marking_bitmap_);
  }

  template <AccessMode mode>
  ConcurrentBitmap<mode>* young_generation_bitmap() const {
    return reinterpret_cast<ConcurrentBitmap<mode>*>(young_generation_bitmap_);
  }

  // If the chunk needs to remember its memory reservation, it is stored here.
  VirtualMemory reservation_;

  // The space owning this memory chunk.
  std::atomic<Space*> owner_;

  // Used by the incremental marker to keep track of the scanning progress in
  // large objects that have a progress bar and are scanned in increments.
  std::atomic<size_t> progress_bar_;

  // Count of bytes marked black on page.
  intptr_t live_byte_count_;

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* sweeping_slot_set_;
  TypedSlotSet* typed_slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES];
  InvalidatedSlots* invalidated_slots_[NUMBER_OF_REMEMBERED_SET_TYPES];

  // Assuming the initial allocation on a page is sequential,
  // count highest number of bytes ever allocated on the page.
  std::atomic<intptr_t> high_water_mark_;

  base::Mutex* mutex_;

  std::atomic<ConcurrentSweepingState> concurrent_sweeping_;

  base::Mutex* page_protection_change_mutex_;

  // This field is only relevant for code pages. It depicts the number of
  // times a component requested this page to be read+writeable. The
  // counter is decremented when a component resets to read+executable.
  // If Value() == 0 => The memory is read and executable.
  // If Value() >= 1 => The Memory is read and writable (and maybe executable).
  // The maximum value is limited by {kMaxWriteUnprotectCounter} to prevent
  // excessive nesting of scopes.
  // All executable MemoryChunks are allocated rw based on the assumption that
  // they will be used immediatelly for an allocation. They are initialized
  // with the number of open CodeSpaceMemoryModificationScopes. The caller
  // that triggers the page allocation is responsible for decrementing the
  // counter.
  uintptr_t write_unprotect_counter_;

  // Byte allocated on the page, which includes all objects on the page
  // and the linear allocation area.
  size_t allocated_bytes_;

  // Tracks off-heap memory used by this memory chunk.
  std::atomic<size_t> external_backing_store_bytes_[kNumTypes];

  // Freed memory that was not added to the free list.
  size_t wasted_memory_;

  base::ListNode<MemoryChunk> list_node_;

  FreeListCategory** categories_;

  LocalArrayBufferTracker* local_tracker_;

  std::atomic<intptr_t> young_generation_live_byte_count_;
  Bitmap* young_generation_bitmap_;

  CodeObjectRegistry* code_object_registry_;

  PossiblyEmptyBuckets possibly_empty_buckets_;

 private:
  void InitializeReservedMemory() { reservation_.Reset(); }

  friend class ConcurrentMarkingState;
  friend class MajorMarkingState;
  friend class MajorAtomicMarkingState;
  friend class MajorNonAtomicMarkingState;
  friend class MemoryAllocator;
  friend class MinorMarkingState;
  friend class MinorNonAtomicMarkingState;
  friend class PagedSpace;
};

STATIC_ASSERT(sizeof(std::atomic<intptr_t>) == kSystemPointerSize);

// -----------------------------------------------------------------------------
// A page is a memory chunk of a size 256K. Large object pages may be larger.
//
// The only way to get a page pointer is by calling factory methods:
//   Page* p = Page::FromAddress(addr); or
//   Page* p = Page::FromAllocationAreaAddress(address);
class Page : public MemoryChunk {
 public:
  static const intptr_t kCopyAllFlags = ~0;

  // Page flags copied from from-space to to-space when flipping semispaces.
  static const intptr_t kCopyOnFlipFlagsMask =
      static_cast<intptr_t>(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING) |
      static_cast<intptr_t>(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING) |
      static_cast<intptr_t>(MemoryChunk::INCREMENTAL_MARKING);

  // Returns the page containing a given address. The address ranges
  // from [page_addr .. page_addr + kPageSize[. This only works if the object
  // is in fact in a page.
  static Page* FromAddress(Address addr) {
    return reinterpret_cast<Page*>(addr & ~kPageAlignmentMask);
  }
  static Page* FromHeapObject(HeapObject o) {
    return reinterpret_cast<Page*>(o.ptr() & ~kAlignmentMask);
  }

  // Returns the page containing the address provided. The address can
  // potentially point righter after the page. To be also safe for tagged values
  // we subtract a hole word. The valid address ranges from
  // [page_addr + area_start_ .. page_addr + kPageSize + kTaggedSize].
  static Page* FromAllocationAreaAddress(Address address) {
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

  inline void MarkNeverAllocateForTesting();
  inline void MarkEvacuationCandidate();
  inline void ClearEvacuationCandidate();

  Page* next_page() { return static_cast<Page*>(list_node_.next()); }
  Page* prev_page() { return static_cast<Page*>(list_node_.prev()); }

  template <typename Callback>
  inline void ForAllFreeListCategories(Callback callback) {
    for (int i = kFirstCategory; i < free_list()->number_of_categories(); i++) {
      callback(categories_[i]);
    }
  }

  // Returns the offset of a given address to this page.
  inline size_t Offset(Address a) { return static_cast<size_t>(a - address()); }

  // Returns the address for a given offset to the this page.
  Address OffsetToAddress(size_t offset) {
    Address address_in_page = address() + offset;
    DCHECK_GE(address_in_page, area_start());
    DCHECK_LT(address_in_page, area_end());
    return address_in_page;
  }

  void AllocateLocalTracker();
  inline LocalArrayBufferTracker* local_tracker() { return local_tracker_; }
  bool contains_array_buffers();

  size_t AvailableInFreeList();

  size_t AvailableInFreeListFromAllocatedBytes() {
    DCHECK_GE(area_size(), wasted_memory() + allocated_bytes());
    return area_size() - wasted_memory() - allocated_bytes();
  }

  FreeListCategory* free_list_category(FreeListCategoryType type) {
    return categories_[type];
  }

  size_t wasted_memory() { return wasted_memory_; }
  void add_wasted_memory(size_t waste) { wasted_memory_ += waste; }
  size_t allocated_bytes() { return allocated_bytes_; }
  void IncreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    allocated_bytes_ += bytes;
  }
  void DecreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    DCHECK_GE(allocated_bytes(), bytes);
    allocated_bytes_ -= bytes;
  }

  void ResetAllocationStatistics();

  size_t ShrinkToHighWaterMark();

  V8_EXPORT_PRIVATE void CreateBlackArea(Address start, Address end);
  void DestroyBlackArea(Address start, Address end);

  void InitializeFreeListCategories();
  void AllocateFreeListCategories();
  void ReleaseFreeListCategories();

  void MoveOldToNewRememberedSetForSweeping();
  void MergeOldToNewRememberedSets();

 private:
  friend class MemoryAllocator;
};

class ReadOnlyPage : public Page {
 public:
  // Clears any pointers in the header that point out of the page that would
  // otherwise make the header non-relocatable.
  void MakeHeaderRelocatable();

 private:
  friend class ReadOnlySpace;
};

class LargePage : public MemoryChunk {
 public:
  // A limit to guarantee that we do not overflow typed slot offset in
  // the old to old remembered set.
  // Note that this limit is higher than what assembler already imposes on
  // x64 and ia32 architectures.
  static const int kMaxCodePageSize = 512 * MB;

  static LargePage* FromHeapObject(HeapObject o) {
    return static_cast<LargePage*>(MemoryChunk::FromHeapObject(o));
  }

  inline HeapObject GetObject();

  inline LargePage* next_page() {
    return static_cast<LargePage*>(list_node_.next());
  }

  // Uncommit memory that is not in use anymore by the object. If the object
  // cannot be shrunk 0 is returned.
  Address GetAddressToShrink(Address object_address, size_t object_size);

  void ClearOutOfLiveRangeSlots(Address free_start);

 private:
  static LargePage* Initialize(Heap* heap, MemoryChunk* chunk,
                               Executability executable);

  friend class MemoryAllocator;
};

// Validate our estimates on the header size.
STATIC_ASSERT(sizeof(BasicMemoryChunk) <= BasicMemoryChunk::kHeaderSize);
STATIC_ASSERT(sizeof(MemoryChunk) <= MemoryChunk::kHeaderSize);
STATIC_ASSERT(sizeof(LargePage) <= MemoryChunk::kHeaderSize);
STATIC_ASSERT(sizeof(Page) <= MemoryChunk::kHeaderSize);

// The process-wide singleton that keeps track of code range regions with the
// intention to reuse free code range regions as a workaround for CFG memory
// leaks (see crbug.com/870054).
class CodeRangeAddressHint {
 public:
  // Returns the most recently freed code range start address for the given
  // size. If there is no such entry, then a random address is returned.
  V8_EXPORT_PRIVATE Address GetAddressHint(size_t code_range_size);

  V8_EXPORT_PRIVATE void NotifyFreedCodeRange(Address code_range_start,
                                              size_t code_range_size);

 private:
  base::Mutex mutex_;
  // A map from code range size to an array of recently freed code range
  // addresses. There should be O(1) different code range sizes.
  // The length of each array is limited by the peak number of code ranges,
  // which should be also O(1).
  std::unordered_map<size_t, std::vector<Address>> recently_freed_;
};

// ----------------------------------------------------------------------------
// A space acquires chunks of memory from the operating system. The memory
// allocator allocates and deallocates pages for the paged heap spaces and large
// pages for large object space.
class MemoryAllocator {
 public:
  // Unmapper takes care of concurrently unmapping and uncommitting memory
  // chunks.
  class Unmapper {
   public:
    class UnmapFreeMemoryTask;

    Unmapper(Heap* heap, MemoryAllocator* allocator)
        : heap_(heap),
          allocator_(allocator),
          pending_unmapping_tasks_semaphore_(0),
          pending_unmapping_tasks_(0),
          active_unmapping_tasks_(0) {
      chunks_[kRegular].reserve(kReservedQueueingSlots);
      chunks_[kPooled].reserve(kReservedQueueingSlots);
    }

    void AddMemoryChunkSafe(MemoryChunk* chunk) {
      if (!chunk->IsLargePage() && chunk->executable() != EXECUTABLE) {
        AddMemoryChunkSafe<kRegular>(chunk);
      } else {
        AddMemoryChunkSafe<kNonRegular>(chunk);
      }
    }

    MemoryChunk* TryGetPooledMemoryChunkSafe() {
      // Procedure:
      // (1) Try to get a chunk that was declared as pooled and already has
      // been uncommitted.
      // (2) Try to steal any memory chunk of kPageSize that would've been
      // unmapped.
      MemoryChunk* chunk = GetMemoryChunkSafe<kPooled>();
      if (chunk == nullptr) {
        chunk = GetMemoryChunkSafe<kRegular>();
        if (chunk != nullptr) {
          // For stolen chunks we need to manually free any allocated memory.
          chunk->ReleaseAllAllocatedMemory();
        }
      }
      return chunk;
    }

    V8_EXPORT_PRIVATE void FreeQueuedChunks();
    void CancelAndWaitForPendingTasks();
    void PrepareForGC();
    V8_EXPORT_PRIVATE void EnsureUnmappingCompleted();
    V8_EXPORT_PRIVATE void TearDown();
    size_t NumberOfCommittedChunks();
    V8_EXPORT_PRIVATE int NumberOfChunks();
    size_t CommittedBufferedMemory();

   private:
    static const int kReservedQueueingSlots = 64;
    static const int kMaxUnmapperTasks = 4;

    enum ChunkQueueType {
      kRegular,     // Pages of kPageSize that do not live in a CodeRange and
                    // can thus be used for stealing.
      kNonRegular,  // Large chunks and executable chunks.
      kPooled,      // Pooled chunks, already uncommited and ready for reuse.
      kNumberOfChunkQueues,
    };

    enum class FreeMode {
      kUncommitPooled,
      kReleasePooled,
    };

    template <ChunkQueueType type>
    void AddMemoryChunkSafe(MemoryChunk* chunk) {
      base::MutexGuard guard(&mutex_);
      chunks_[type].push_back(chunk);
    }

    template <ChunkQueueType type>
    MemoryChunk* GetMemoryChunkSafe() {
      base::MutexGuard guard(&mutex_);
      if (chunks_[type].empty()) return nullptr;
      MemoryChunk* chunk = chunks_[type].back();
      chunks_[type].pop_back();
      return chunk;
    }

    bool MakeRoomForNewTasks();

    template <FreeMode mode>
    void PerformFreeMemoryOnQueuedChunks();

    void PerformFreeMemoryOnQueuedNonRegularChunks();

    Heap* const heap_;
    MemoryAllocator* const allocator_;
    base::Mutex mutex_;
    std::vector<MemoryChunk*> chunks_[kNumberOfChunkQueues];
    CancelableTaskManager::Id task_ids_[kMaxUnmapperTasks];
    base::Semaphore pending_unmapping_tasks_semaphore_;
    intptr_t pending_unmapping_tasks_;
    std::atomic<intptr_t> active_unmapping_tasks_;

    friend class MemoryAllocator;
  };

  enum AllocationMode {
    kRegular,
    kPooled,
  };

  enum FreeMode {
    kFull,
    kAlreadyPooled,
    kPreFreeAndQueue,
    kPooledAndQueue,
  };

  V8_EXPORT_PRIVATE static intptr_t GetCommitPageSize();

  // Computes the memory area of discardable memory within a given memory area
  // [addr, addr+size) and returns the result as base::AddressRegion. If the
  // memory is not discardable base::AddressRegion is an empty region.
  V8_EXPORT_PRIVATE static base::AddressRegion ComputeDiscardMemoryArea(
      Address addr, size_t size);

  V8_EXPORT_PRIVATE MemoryAllocator(Isolate* isolate, size_t max_capacity,
                                    size_t code_range_size);

  V8_EXPORT_PRIVATE void TearDown();

  // Allocates a Page from the allocator. AllocationMode is used to indicate
  // whether pooled allocation, which only works for MemoryChunk::kPageSize,
  // should be tried first.
  template <MemoryAllocator::AllocationMode alloc_mode = kRegular,
            typename SpaceType>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Page* AllocatePage(size_t size, SpaceType* owner, Executability executable);

  LargePage* AllocateLargePage(size_t size, LargeObjectSpace* owner,
                               Executability executable);

  template <MemoryAllocator::FreeMode mode = kFull>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  void Free(MemoryChunk* chunk);

  // Returns allocated spaces in bytes.
  size_t Size() { return size_; }

  // Returns allocated executable spaces in bytes.
  size_t SizeExecutable() { return size_executable_; }

  // Returns the maximum available bytes of heaps.
  size_t Available() {
    const size_t size = Size();
    return capacity_ < size ? 0 : capacity_ - size;
  }

  // Returns an indication of whether a pointer is in a space that has
  // been allocated by this MemoryAllocator.
  V8_INLINE bool IsOutsideAllocatedSpace(Address address) {
    return address < lowest_ever_allocated_ ||
           address >= highest_ever_allocated_;
  }

  // Returns a MemoryChunk in which the memory region from commit_area_size to
  // reserve_area_size of the chunk area is reserved but not committed, it
  // could be committed later by calling MemoryChunk::CommitArea.
  V8_EXPORT_PRIVATE MemoryChunk* AllocateChunk(size_t reserve_area_size,
                                               size_t commit_area_size,
                                               Executability executable,
                                               Space* space);

  Address AllocateAlignedMemory(size_t reserve_size, size_t commit_size,
                                size_t alignment, Executability executable,
                                void* hint, VirtualMemory* controller);

  void FreeMemory(v8::PageAllocator* page_allocator, Address addr, size_t size);

  // Partially release |bytes_to_free| bytes starting at |start_free|. Note that
  // internally memory is freed from |start_free| to the end of the reservation.
  // Additional memory beyond the page is not accounted though, so
  // |bytes_to_free| is computed by the caller.
  void PartialFreeMemory(MemoryChunk* chunk, Address start_free,
                         size_t bytes_to_free, Address new_area_end);

  // Checks if an allocated MemoryChunk was intended to be used for executable
  // memory.
  bool IsMemoryChunkExecutable(MemoryChunk* chunk) {
    return executable_memory_.find(chunk) != executable_memory_.end();
  }

  // Commit memory region owned by given reservation object.  Returns true if
  // it succeeded and false otherwise.
  bool CommitMemory(VirtualMemory* reservation);

  // Uncommit memory region owned by given reservation object. Returns true if
  // it succeeded and false otherwise.
  bool UncommitMemory(VirtualMemory* reservation);

  // Zaps a contiguous block of memory [start..(start+size)[ with
  // a given zap value.
  void ZapBlock(Address start, size_t size, uintptr_t zap_value);

  V8_WARN_UNUSED_RESULT bool CommitExecutableMemory(VirtualMemory* vm,
                                                    Address start,
                                                    size_t commit_size,
                                                    size_t reserved_size);

  // Page allocator instance for allocating non-executable pages.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* data_page_allocator() { return data_page_allocator_; }

  // Page allocator instance for allocating executable pages.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* code_page_allocator() { return code_page_allocator_; }

  // Returns page allocator suitable for allocating pages with requested
  // executability.
  v8::PageAllocator* page_allocator(Executability executable) {
    return executable == EXECUTABLE ? code_page_allocator_
                                    : data_page_allocator_;
  }

  // A region of memory that may contain executable code including reserved
  // OS page with read-write access in the beginning.
  const base::AddressRegion& code_range() const {
    // |code_range_| >= |optional RW pages| + |code_page_allocator_instance_|
    DCHECK_IMPLIES(!code_range_.is_empty(), code_page_allocator_instance_);
    DCHECK_IMPLIES(!code_range_.is_empty(),
                   code_range_.contains(code_page_allocator_instance_->begin(),
                                        code_page_allocator_instance_->size()));
    return code_range_;
  }

  Unmapper* unmapper() { return &unmapper_; }

  // Performs all necessary bookkeeping to free the memory, but does not free
  // it.
  void UnregisterMemory(MemoryChunk* chunk);

 private:
  void InitializeCodePageAllocator(v8::PageAllocator* page_allocator,
                                   size_t requested);

  // PreFreeMemory logically frees the object, i.e., it unregisters the memory,
  // logs a delete event and adds the chunk to remembered unmapped pages.
  void PreFreeMemory(MemoryChunk* chunk);

  // PerformFreeMemory can be called concurrently when PreFree was executed
  // before.
  void PerformFreeMemory(MemoryChunk* chunk);

  // See AllocatePage for public interface. Note that currently we only support
  // pools for NOT_EXECUTABLE pages of size MemoryChunk::kPageSize.
  template <typename SpaceType>
  MemoryChunk* AllocatePagePooled(SpaceType* owner);

  // Initializes pages in a chunk. Returns the first page address.
  // This function and GetChunkId() are provided for the mark-compact
  // collector to rebuild page headers in the from space, which is
  // used as a marking stack and its page headers are destroyed.
  Page* InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                               PagedSpace* owner);

  void UpdateAllocatedSpaceLimits(Address low, Address high) {
    // The use of atomic primitives does not guarantee correctness (wrt.
    // desired semantics) by default. The loop here ensures that we update the
    // values only if they did not change in between.
    Address ptr = kNullAddress;
    do {
      ptr = lowest_ever_allocated_;
    } while ((low < ptr) &&
             !lowest_ever_allocated_.compare_exchange_weak(ptr, low));
    do {
      ptr = highest_ever_allocated_;
    } while ((high > ptr) &&
             !highest_ever_allocated_.compare_exchange_weak(ptr, high));
  }

  void RegisterExecutableMemoryChunk(MemoryChunk* chunk) {
    DCHECK(chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE));
    DCHECK_EQ(executable_memory_.find(chunk), executable_memory_.end());
    executable_memory_.insert(chunk);
  }

  void UnregisterExecutableMemoryChunk(MemoryChunk* chunk) {
    DCHECK_NE(executable_memory_.find(chunk), executable_memory_.end());
    executable_memory_.erase(chunk);
    chunk->heap()->UnregisterUnprotectedMemoryChunk(chunk);
  }

  Isolate* isolate_;

  // This object controls virtual space reserved for V8 heap instance.
  // Depending on the configuration it may contain the following:
  // - no reservation (on 32-bit architectures)
  // - code range reservation used by bounded code page allocator (on 64-bit
  //   architectures without pointers compression in V8 heap)
  // - data + code range reservation (on 64-bit architectures with pointers
  //   compression in V8 heap)
  VirtualMemory heap_reservation_;

  // Page allocator used for allocating data pages. Depending on the
  // configuration it may be a page allocator instance provided by v8::Platform
  // or a BoundedPageAllocator (when pointer compression is enabled).
  v8::PageAllocator* data_page_allocator_;

  // Page allocator used for allocating code pages. Depending on the
  // configuration it may be a page allocator instance provided by v8::Platform
  // or a BoundedPageAllocator (when pointer compression is enabled or
  // on those 64-bit architectures where pc-relative 32-bit displacement
  // can be used for call and jump instructions).
  v8::PageAllocator* code_page_allocator_;

  // A part of the |heap_reservation_| that may contain executable code
  // including reserved page with read-write access in the beginning.
  // See details below.
  base::AddressRegion code_range_;

  // This unique pointer owns the instance of bounded code allocator
  // that controls executable pages allocation. It does not control the
  // optionally existing page in the beginning of the |code_range_|.
  // So, summarizing all above, the following conditions hold:
  // 1) |heap_reservation_| >= |code_range_|
  // 2) |code_range_| >= |optional RW pages| + |code_page_allocator_instance_|.
  // 3) |heap_reservation_| is AllocatePageSize()-aligned
  // 4) |code_page_allocator_instance_| is MemoryChunk::kAlignment-aligned
  // 5) |code_range_| is CommitPageSize()-aligned
  std::unique_ptr<base::BoundedPageAllocator> code_page_allocator_instance_;

  // Maximum space size in bytes.
  size_t capacity_;

  // Allocated space size in bytes.
  std::atomic<size_t> size_;
  // Allocated executable space size in bytes.
  std::atomic<size_t> size_executable_;

  // We keep the lowest and highest addresses allocated as a quick way
  // of determining that pointers are outside the heap. The estimate is
  // conservative, i.e. not all addresses in 'allocated' space are allocated
  // to our heap. The range is [lowest, highest[, inclusive on the low end
  // and exclusive on the high end.
  std::atomic<Address> lowest_ever_allocated_;
  std::atomic<Address> highest_ever_allocated_;

  VirtualMemory last_chunk_;
  Unmapper unmapper_;

  // Data structure to remember allocated executable memory chunks.
  std::unordered_set<MemoryChunk*> executable_memory_;

  friend class heap::TestCodePageAllocatorScope;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryAllocator);
};

extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, PagedSpace>(
        size_t size, PagedSpace* owner, Executability executable);
extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, SemiSpace>(
        size_t size, SemiSpace* owner, Executability executable);
extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kPooled, SemiSpace>(
        size_t size, SemiSpace* owner, Executability executable);

extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kFull>(MemoryChunk* chunk);
extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kAlreadyPooled>(MemoryChunk* chunk);
extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kPreFreeAndQueue>(MemoryChunk* chunk);
extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kPooledAndQueue>(MemoryChunk* chunk);

// -----------------------------------------------------------------------------
// Interface for heap object iterator to be implemented by all object space
// object iterators.

class V8_EXPORT_PRIVATE ObjectIterator : public Malloced {
 public:
  virtual ~ObjectIterator() = default;
  virtual HeapObject Next() = 0;
};

template <class PAGE_TYPE>
class PageIteratorImpl
    : public base::iterator<std::forward_iterator_tag, PAGE_TYPE> {
 public:
  explicit PageIteratorImpl(PAGE_TYPE* p) : p_(p) {}
  PageIteratorImpl(const PageIteratorImpl<PAGE_TYPE>& other) : p_(other.p_) {}
  PAGE_TYPE* operator*() { return p_; }
  bool operator==(const PageIteratorImpl<PAGE_TYPE>& rhs) {
    return rhs.p_ == p_;
  }
  bool operator!=(const PageIteratorImpl<PAGE_TYPE>& rhs) {
    return rhs.p_ != p_;
  }
  inline PageIteratorImpl<PAGE_TYPE>& operator++();
  inline PageIteratorImpl<PAGE_TYPE> operator++(int);

 private:
  PAGE_TYPE* p_;
};

using PageIterator = PageIteratorImpl<Page>;
using LargePageIterator = PageIteratorImpl<LargePage>;

class PageRange {
 public:
  using iterator = PageIterator;
  PageRange(Page* begin, Page* end) : begin_(begin), end_(end) {}
  explicit PageRange(Page* page) : PageRange(page, page->next_page()) {}
  inline PageRange(Address start, Address limit);

  iterator begin() { return iterator(begin_); }
  iterator end() { return iterator(end_); }

 private:
  Page* begin_;
  Page* end_;
};

// -----------------------------------------------------------------------------
// Heap object iterator in new/old/map spaces.
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

  // Creates a new object iterator in a given off-thread space.
  explicit PagedSpaceObjectIterator(OffThreadSpace* space);

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

// -----------------------------------------------------------------------------
// A space has a circular list of pages. The next page can be accessed via
// Page::next_page() call.

// An abstraction of allocation and relocation pointers in a page-structured
// space.
class LinearAllocationArea {
 public:
  LinearAllocationArea() : top_(kNullAddress), limit_(kNullAddress) {}
  LinearAllocationArea(Address top, Address limit) : top_(top), limit_(limit) {}

  void Reset(Address top, Address limit) {
    set_top(top);
    set_limit(limit);
  }

  V8_INLINE void set_top(Address top) {
    SLOW_DCHECK(top == kNullAddress || (top & kHeapObjectTagMask) == 0);
    top_ = top;
  }

  V8_INLINE Address top() const {
    SLOW_DCHECK(top_ == kNullAddress || (top_ & kHeapObjectTagMask) == 0);
    return top_;
  }

  Address* top_address() { return &top_; }

  V8_INLINE void set_limit(Address limit) { limit_ = limit; }

  V8_INLINE Address limit() const { return limit_; }

  Address* limit_address() { return &limit_; }

#ifdef DEBUG
  bool VerifyPagedAllocation() {
    return (Page::FromAllocationAreaAddress(top_) ==
            Page::FromAllocationAreaAddress(limit_)) &&
           (top_ <= limit_);
  }
#endif

 private:
  // Current allocation top.
  Address top_;
  // Current allocation limit.
  Address limit_;
};

// An abstraction of the accounting statistics of a page-structured space.
//
// The stats are only set by functions that ensure they stay balanced. These
// functions increase or decrease one of the non-capacity stats in conjunction
// with capacity, or else they always balance increases and decreases to the
// non-capacity stats.
class AllocationStats {
 public:
  AllocationStats() { Clear(); }

  // Zero out all the allocation statistics (i.e., no capacity).
  void Clear() {
    capacity_ = 0;
    max_capacity_ = 0;
    ClearSize();
  }

  void ClearSize() {
    size_ = 0;
#ifdef DEBUG
    allocated_on_page_.clear();
#endif
  }

  // Accessors for the allocation statistics.
  size_t Capacity() { return capacity_; }
  size_t MaxCapacity() { return max_capacity_; }
  size_t Size() { return size_; }
#ifdef DEBUG
  size_t AllocatedOnPage(Page* page) { return allocated_on_page_[page]; }
#endif

  void IncreaseAllocatedBytes(size_t bytes, Page* page) {
    DCHECK_GE(size_ + bytes, size_);
    size_ += bytes;
#ifdef DEBUG
    allocated_on_page_[page] += bytes;
#endif
  }

  void DecreaseAllocatedBytes(size_t bytes, Page* page) {
    DCHECK_GE(size_, bytes);
    size_ -= bytes;
#ifdef DEBUG
    DCHECK_GE(allocated_on_page_[page], bytes);
    allocated_on_page_[page] -= bytes;
#endif
  }

  void DecreaseCapacity(size_t bytes) {
    DCHECK_GE(capacity_, bytes);
    DCHECK_GE(capacity_ - bytes, size_);
    capacity_ -= bytes;
  }

  void IncreaseCapacity(size_t bytes) {
    DCHECK_GE(capacity_ + bytes, capacity_);
    capacity_ += bytes;
    if (capacity_ > max_capacity_) {
      max_capacity_ = capacity_;
    }
  }

 private:
  // |capacity_|: The number of object-area bytes (i.e., not including page
  // bookkeeping structures) currently in the space.
  // During evacuation capacity of the main spaces is accessed from multiple
  // threads to check the old generation hard limit.
  std::atomic<size_t> capacity_;

  // |max_capacity_|: The maximum capacity ever observed.
  size_t max_capacity_;

  // |size_|: The number of allocated bytes.
  size_t size_;

#ifdef DEBUG
  std::unordered_map<Page*, size_t, Page::Hasher> allocated_on_page_;
#endif
};

// The free list is organized in categories as follows:
// kMinBlockSize-10 words (tiniest): The tiniest blocks are only used for
//   allocation, when categories >= small do not have entries anymore.
// 11-31 words (tiny): The tiny blocks are only used for allocation, when
//   categories >= small do not have entries anymore.
// 32-255 words (small): Used for allocating free space between 1-31 words in
//   size.
// 256-2047 words (medium): Used for allocating free space between 32-255 words
//   in size.
// 1048-16383 words (large): Used for allocating free space between 256-2047
//   words in size.
// At least 16384 words (huge): This list is for objects of 2048 words or
//   larger. Empty pages are also added to this list.
class V8_EXPORT_PRIVATE FreeListLegacy : public FreeList {
 public:
  size_t GuaranteedAllocatable(size_t maximum_freed) override {
    if (maximum_freed <= kTiniestListMax) {
      // Since we are not iterating over all list entries, we cannot guarantee
      // that we can find the maximum freed block in that free list.
      return 0;
    } else if (maximum_freed <= kTinyListMax) {
      return kTinyAllocationMax;
    } else if (maximum_freed <= kSmallListMax) {
      return kSmallAllocationMax;
    } else if (maximum_freed <= kMediumListMax) {
      return kMediumAllocationMax;
    } else if (maximum_freed <= kLargeListMax) {
      return kLargeAllocationMax;
    }
    return maximum_freed;
  }

  inline Page* GetPageForSize(size_t size_in_bytes) override;

  FreeListLegacy();
  ~FreeListLegacy();

  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;

 private:
  enum { kTiniest, kTiny, kSmall, kMedium, kLarge, kHuge };

  static const size_t kMinBlockSize = 3 * kTaggedSize;

  // This is a conservative upper bound. The actual maximum block size takes
  // padding and alignment of data and code pages into account.
  static const size_t kMaxBlockSize = Page::kPageSize;

  static const size_t kTiniestListMax = 0xa * kTaggedSize;
  static const size_t kTinyListMax = 0x1f * kTaggedSize;
  static const size_t kSmallListMax = 0xff * kTaggedSize;
  static const size_t kMediumListMax = 0x7ff * kTaggedSize;
  static const size_t kLargeListMax = 0x1fff * kTaggedSize;
  static const size_t kTinyAllocationMax = kTiniestListMax;
  static const size_t kSmallAllocationMax = kTinyListMax;
  static const size_t kMediumAllocationMax = kSmallListMax;
  static const size_t kLargeAllocationMax = kMediumListMax;

  FreeListCategoryType SelectFreeListCategoryType(
      size_t size_in_bytes) override {
    if (size_in_bytes <= kTiniestListMax) {
      return kTiniest;
    } else if (size_in_bytes <= kTinyListMax) {
      return kTiny;
    } else if (size_in_bytes <= kSmallListMax) {
      return kSmall;
    } else if (size_in_bytes <= kMediumListMax) {
      return kMedium;
    } else if (size_in_bytes <= kLargeListMax) {
      return kLarge;
    }
    return kHuge;
  }

  // Returns the category to be used to allocate |size_in_bytes| in the fast
  // path. The tiny categories are not used for fast allocation.
  FreeListCategoryType SelectFastAllocationFreeListCategoryType(
      size_t size_in_bytes) {
    if (size_in_bytes <= kSmallAllocationMax) {
      return kSmall;
    } else if (size_in_bytes <= kMediumAllocationMax) {
      return kMedium;
    } else if (size_in_bytes <= kLargeAllocationMax) {
      return kLarge;
    }
    return kHuge;
  }

  friend class FreeListCategory;
  friend class heap::HeapTester;
};

// Inspired by FreeListLegacy.
// Only has 3 categories: Medium, Large and Huge.
// Any block that would have belong to tiniest, tiny or small in FreeListLegacy
// is considered wasted.
// Allocation is done only in Huge, Medium and Large (in that order),
// using a first-fit strategy (only the first block of each freelist is ever
// considered though). Performances is supposed to be better than
// FreeListLegacy, but memory usage should be higher (because fragmentation will
// probably be higher).
class V8_EXPORT_PRIVATE FreeListFastAlloc : public FreeList {
 public:
  size_t GuaranteedAllocatable(size_t maximum_freed) override {
    if (maximum_freed <= kMediumListMax) {
      // Since we are not iterating over all list entries, we cannot guarantee
      // that we can find the maximum freed block in that free list.
      return 0;
    } else if (maximum_freed <= kLargeListMax) {
      return kLargeAllocationMax;
    }
    return kHugeAllocationMax;
  }

  inline Page* GetPageForSize(size_t size_in_bytes) override;

  FreeListFastAlloc();
  ~FreeListFastAlloc();

  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;

 private:
  enum { kMedium, kLarge, kHuge };

  static const size_t kMinBlockSize = 0xff * kTaggedSize;

  // This is a conservative upper bound. The actual maximum block size takes
  // padding and alignment of data and code pages into account.
  static const size_t kMaxBlockSize = Page::kPageSize;

  static const size_t kMediumListMax = 0x7ff * kTaggedSize;
  static const size_t kLargeListMax = 0x1fff * kTaggedSize;
  static const size_t kMediumAllocationMax = kMinBlockSize;
  static const size_t kLargeAllocationMax = kMediumListMax;
  static const size_t kHugeAllocationMax = kLargeListMax;

  // Returns the category used to hold an object of size |size_in_bytes|.
  FreeListCategoryType SelectFreeListCategoryType(
      size_t size_in_bytes) override {
    if (size_in_bytes <= kMediumListMax) {
      return kMedium;
    } else if (size_in_bytes <= kLargeListMax) {
      return kLarge;
    }
    return kHuge;
  }
};

// Use 24 Freelists: on per 16 bytes between 24 and 256, and then a few ones for
// larger sizes. See the variable |categories_min| for the size of each
// Freelist.  Allocation is done using a best-fit strategy (considering only the
// first element of each category though).
// Performances are expected to be worst than FreeListLegacy, but memory
// consumption should be lower (since fragmentation should be lower).
class V8_EXPORT_PRIVATE FreeListMany : public FreeList {
 public:
  size_t GuaranteedAllocatable(size_t maximum_freed) override;

  Page* GetPageForSize(size_t size_in_bytes) override;

  FreeListMany();
  ~FreeListMany();

  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;

 protected:
  static const size_t kMinBlockSize = 3 * kTaggedSize;

  // This is a conservative upper bound. The actual maximum block size takes
  // padding and alignment of data and code pages into account.
  static const size_t kMaxBlockSize = Page::kPageSize;
  // Largest size for which categories are still precise, and for which we can
  // therefore compute the category in constant time.
  static const size_t kPreciseCategoryMaxSize = 256;

  // Categories boundaries generated with:
  // perl -E '
  //      @cat = (24, map {$_*16} 2..16, 48, 64);
  //      while ($cat[-1] <= 32768) {
  //        push @cat, $cat[-1]*2
  //      }
  //      say join ", ", @cat;
  //      say "\n", scalar @cat'
  static const int kNumberOfCategories = 24;
  static constexpr unsigned int categories_min[kNumberOfCategories] = {
      24,  32,  48,  64,  80,  96,   112,  128,  144,  160,   176,   192,
      208, 224, 240, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

  // Return the smallest category that could hold |size_in_bytes| bytes.
  FreeListCategoryType SelectFreeListCategoryType(
      size_t size_in_bytes) override {
    if (size_in_bytes <= kPreciseCategoryMaxSize) {
      if (size_in_bytes < categories_min[1]) return 0;
      return static_cast<FreeListCategoryType>(size_in_bytes >> 4) - 1;
    }
    for (int cat = (kPreciseCategoryMaxSize >> 4) - 1; cat < last_category_;
         cat++) {
      if (size_in_bytes < categories_min[cat + 1]) {
        return cat;
      }
    }
    return last_category_;
  }

  FRIEND_TEST(SpacesTest, FreeListManySelectFreeListCategoryType);
  FRIEND_TEST(SpacesTest, FreeListManyGuaranteedAllocatable);
};

// Same as FreeListMany but uses a cache to know which categories are empty.
// The cache (|next_nonempty_category|) is maintained in a way such that for
// each category c, next_nonempty_category[c] contains the first non-empty
// category greater or equal to c, that may hold an object of size c.
// Allocation is done using the same strategy as FreeListMany (ie, best fit).
class V8_EXPORT_PRIVATE FreeListManyCached : public FreeListMany {
 public:
  FreeListManyCached();

  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;

  size_t Free(Address start, size_t size_in_bytes, FreeMode mode) override;

  void Reset() override;

  bool AddCategory(FreeListCategory* category) override;
  void RemoveCategory(FreeListCategory* category) override;

 protected:
  // Updates the cache after adding something in the category |cat|.
  void UpdateCacheAfterAddition(FreeListCategoryType cat) {
    for (int i = cat; i >= kFirstCategory && next_nonempty_category[i] > cat;
         i--) {
      next_nonempty_category[i] = cat;
    }
  }

  // Updates the cache after emptying category |cat|.
  void UpdateCacheAfterRemoval(FreeListCategoryType cat) {
    for (int i = cat; i >= kFirstCategory && next_nonempty_category[i] == cat;
         i--) {
      next_nonempty_category[i] = next_nonempty_category[cat + 1];
    }
  }

#ifdef DEBUG
  void CheckCacheIntegrity() {
    for (int i = 0; i <= last_category_; i++) {
      DCHECK(next_nonempty_category[i] == last_category_ + 1 ||
             categories_[next_nonempty_category[i]] != nullptr);
      for (int j = i; j < next_nonempty_category[i]; j++) {
        DCHECK(categories_[j] == nullptr);
      }
    }
  }
#endif

  // The cache is overallocated by one so that the last element is always
  // defined, and when updating the cache, we can always use cache[i+1] as long
  // as i is < kNumberOfCategories.
  int next_nonempty_category[kNumberOfCategories + 1];

 private:
  void ResetCache() {
    for (int i = 0; i < kNumberOfCategories; i++) {
      next_nonempty_category[i] = kNumberOfCategories;
    }
    // Setting the after-last element as well, as explained in the cache's
    // declaration.
    next_nonempty_category[kNumberOfCategories] = kNumberOfCategories;
  }
};

// Same as FreeListManyCached but uses a fast path.
// The fast path overallocates by at least 1.85k bytes. The idea of this 1.85k
// is: we want the fast path to always overallocate, even for larger
// categories. Therefore, we have two choices: either overallocate by
// "size_in_bytes * something" or overallocate by "size_in_bytes +
// something". We choose the later, as the former will tend to overallocate too
// much for larger objects. The 1.85k (= 2048 - 128) has been chosen such that
// for tiny objects (size <= 128 bytes), the first category considered is the
// 36th (which holds objects of 2k to 3k), while for larger objects, the first
// category considered will be one that guarantees a 1.85k+ bytes
// overallocation. Using 2k rather than 1.85k would have resulted in either a
// more complex logic for SelectFastAllocationFreeListCategoryType, or the 36th
// category (2k to 3k) not being used; both of which are undesirable.
// A secondary fast path is used for tiny objects (size <= 128), in order to
// consider categories from 256 to 2048 bytes for them.
// Note that this class uses a precise GetPageForSize (inherited from
// FreeListMany), which makes its fast path less fast in the Scavenger. This is
// done on purpose, since this class's only purpose is to be used by
// FreeListManyCachedOrigin, which is precise for the scavenger.
class V8_EXPORT_PRIVATE FreeListManyCachedFastPath : public FreeListManyCached {
 public:
  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;

 protected:
  // Objects in the 18th category are at least 2048 bytes
  static const FreeListCategoryType kFastPathFirstCategory = 18;
  static const size_t kFastPathStart = 2048;
  static const size_t kTinyObjectMaxSize = 128;
  static const size_t kFastPathOffset = kFastPathStart - kTinyObjectMaxSize;
  // Objects in the 15th category are at least 256 bytes
  static const FreeListCategoryType kFastPathFallBackTiny = 15;

  STATIC_ASSERT(categories_min[kFastPathFirstCategory] == kFastPathStart);
  STATIC_ASSERT(categories_min[kFastPathFallBackTiny] ==
                kTinyObjectMaxSize * 2);

  FreeListCategoryType SelectFastAllocationFreeListCategoryType(
      size_t size_in_bytes) {
    DCHECK(size_in_bytes < kMaxBlockSize);

    if (size_in_bytes >= categories_min[last_category_]) return last_category_;

    size_in_bytes += kFastPathOffset;
    for (int cat = kFastPathFirstCategory; cat < last_category_; cat++) {
      if (size_in_bytes <= categories_min[cat]) {
        return cat;
      }
    }
    return last_category_;
  }

  FRIEND_TEST(
      SpacesTest,
      FreeListManyCachedFastPathSelectFastAllocationFreeListCategoryType);
};

// Uses FreeListManyCached if in the GC; FreeListManyCachedFastPath otherwise.
// The reasonning behind this FreeList is the following: the GC runs in
// parallel, and therefore, more expensive allocations there are less
// noticeable. On the other hand, the generated code and runtime need to be very
// fast. Therefore, the strategy for the former is one that is not very
// efficient, but reduces fragmentation (FreeListManyCached), while the strategy
// for the later is one that is very efficient, but introduces some
// fragmentation (FreeListManyCachedFastPath).
class V8_EXPORT_PRIVATE FreeListManyCachedOrigin
    : public FreeListManyCachedFastPath {
 public:
  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;
};

// FreeList for maps: since maps are all the same size, uses a single freelist.
class V8_EXPORT_PRIVATE FreeListMap : public FreeList {
 public:
  size_t GuaranteedAllocatable(size_t maximum_freed) override;

  Page* GetPageForSize(size_t size_in_bytes) override;

  FreeListMap();
  ~FreeListMap();

  V8_WARN_UNUSED_RESULT FreeSpace Allocate(size_t size_in_bytes,
                                           size_t* node_size,
                                           AllocationOrigin origin) override;

 private:
  static const size_t kMinBlockSize = Map::kSize;
  static const size_t kMaxBlockSize = Page::kPageSize;
  static const FreeListCategoryType kOnlyCategory = 0;

  FreeListCategoryType SelectFreeListCategoryType(
      size_t size_in_bytes) override {
    return kOnlyCategory;
  }
};

// LocalAllocationBuffer represents a linear allocation area that is created
// from a given {AllocationResult} and can be used to allocate memory without
// synchronization.
//
// The buffer is properly closed upon destruction and reassignment.
// Example:
//   {
//     AllocationResult result = ...;
//     LocalAllocationBuffer a(heap, result, size);
//     LocalAllocationBuffer b = a;
//     CHECK(!a.IsValid());
//     CHECK(b.IsValid());
//     // {a} is invalid now and cannot be used for further allocations.
//   }
//   // Since {b} went out of scope, the LAB is closed, resulting in creating a
//   // filler object for the remaining area.
class LocalAllocationBuffer {
 public:
  // Indicates that a buffer cannot be used for allocations anymore. Can result
  // from either reassigning a buffer, or trying to construct it from an
  // invalid {AllocationResult}.
  static LocalAllocationBuffer InvalidBuffer() {
    return LocalAllocationBuffer(
        nullptr, LinearAllocationArea(kNullAddress, kNullAddress));
  }

  // Creates a new LAB from a given {AllocationResult}. Results in
  // InvalidBuffer if the result indicates a retry.
  static inline LocalAllocationBuffer FromResult(Heap* heap,
                                                 AllocationResult result,
                                                 intptr_t size);

  ~LocalAllocationBuffer() { Close(); }

  // Convert to C++11 move-semantics once allowed by the style guide.
  LocalAllocationBuffer(const LocalAllocationBuffer& other) V8_NOEXCEPT;
  LocalAllocationBuffer& operator=(const LocalAllocationBuffer& other)
      V8_NOEXCEPT;

  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment);

  inline bool IsValid() { return allocation_info_.top() != kNullAddress; }

  // Try to merge LABs, which is only possible when they are adjacent in memory.
  // Returns true if the merge was successful, false otherwise.
  inline bool TryMerge(LocalAllocationBuffer* other);

  inline bool TryFreeLast(HeapObject object, int object_size);

  // Close a LAB, effectively invalidating it. Returns the unused area.
  V8_EXPORT_PRIVATE LinearAllocationArea Close();

 private:
  V8_EXPORT_PRIVATE LocalAllocationBuffer(
      Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT;

  Heap* heap_;
  LinearAllocationArea allocation_info_;
};

class SpaceWithLinearArea : public Space {
 public:
  SpaceWithLinearArea(Heap* heap, AllocationSpace id, FreeList* free_list)
      : Space(heap, id, free_list), top_on_previous_step_(0) {
    allocation_info_.Reset(kNullAddress, kNullAddress);
  }

  virtual bool SupportsInlineAllocation() = 0;

  // Returns the allocation pointer in this space.
  Address top() { return allocation_info_.top(); }
  Address limit() { return allocation_info_.limit(); }

  // The allocation top address.
  Address* allocation_top_address() { return allocation_info_.top_address(); }

  // The allocation limit address.
  Address* allocation_limit_address() {
    return allocation_info_.limit_address();
  }

  V8_EXPORT_PRIVATE void AddAllocationObserver(
      AllocationObserver* observer) override;
  V8_EXPORT_PRIVATE void RemoveAllocationObserver(
      AllocationObserver* observer) override;
  V8_EXPORT_PRIVATE void ResumeAllocationObservers() override;
  V8_EXPORT_PRIVATE void PauseAllocationObservers() override;

  // When allocation observers are active we may use a lower limit to allow the
  // observers to 'interrupt' earlier than the natural limit. Given a linear
  // area bounded by [start, end), this function computes the limit to use to
  // allow proper observation based on existing observers. min_size specifies
  // the minimum size that the limited area should have.
  Address ComputeLimit(Address start, Address end, size_t min_size);
  V8_EXPORT_PRIVATE virtual void UpdateInlineAllocationLimit(
      size_t min_size) = 0;

  V8_EXPORT_PRIVATE void UpdateAllocationOrigins(AllocationOrigin origin);

  void PrintAllocationsOrigins();

 protected:
  // If we are doing inline allocation in steps, this method performs the 'step'
  // operation. top is the memory address of the bump pointer at the last
  // inline allocation (i.e. it determines the numbers of bytes actually
  // allocated since the last step.) top_for_next_step is the address of the
  // bump pointer where the next byte is going to be allocated from. top and
  // top_for_next_step may be different when we cross a page boundary or reset
  // the space.
  // TODO(ofrobots): clarify the precise difference between this and
  // Space::AllocationStep.
  void InlineAllocationStep(Address top, Address top_for_next_step,
                            Address soon_object, size_t size);
  V8_EXPORT_PRIVATE void StartNextInlineAllocationStep() override;

  // TODO(ofrobots): make these private after refactoring is complete.
  LinearAllocationArea allocation_info_;
  Address top_on_previous_step_;

  size_t allocations_origins_[static_cast<int>(
      AllocationOrigin::kNumberOfAllocationOrigins)] = {0};
};

class V8_EXPORT_PRIVATE PagedSpace
    : NON_EXPORTED_BASE(public SpaceWithLinearArea) {
 public:
  using iterator = PageIterator;

  static const size_t kCompactionMemoryWanted = 500 * KB;

  // Creates a space with an id.
  PagedSpace(Heap* heap, AllocationSpace id, Executability executable,
             FreeList* free_list,
             LocalSpaceKind local_space_kind = LocalSpaceKind::kNone);

  ~PagedSpace() override { TearDown(); }

  // Checks whether an object/address is in this space.
  inline bool Contains(Address a);
  inline bool Contains(Object o);
  bool ContainsSlow(Address addr);

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
  size_t Available() override { return free_list_->Available(); }

  // Allocated bytes in this space.  Garbage bytes that were not found due to
  // concurrent sweeping are counted as being allocated!  The bytes in the
  // current linear allocation area (between top and limit) are also counted
  // here.
  size_t Size() override { return accounting_stats_.Size(); }

  // As size, but the bytes in lazily swept pages are estimated and the bytes
  // in the current linear allocation area are not included.
  size_t SizeOfObjects() override;

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

  size_t Free(Address start, size_t size_in_bytes, SpaceAccountingMode mode) {
    if (size_in_bytes == 0) return 0;
    heap()->CreateFillerObjectAt(start, static_cast<int>(size_in_bytes),
                                 ClearRecordedSlots::kNo);
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

  bool is_off_thread_space() {
    return local_space_kind_ == LocalSpaceKind::kOffThreadSpace;
  }

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

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  // Shrink immortal immovable pages of the space to be exactly the size needed
  // using the high water mark.
  void ShrinkImmortalImmovablePages();

  size_t ShrinkPageToHighWaterMark(Page* page);

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

  void SetLinearAllocationArea(Address top, Address limit);

 private:
  // Set space linear allocation area.
  void SetTopAndLimit(Address top, Address limit) {
    DCHECK(top == limit ||
           Page::FromAddress(top) == Page::FromAddress(limit - 1));
    MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
    allocation_info_.Reset(top, limit);
  }
  void DecreaseLimit(Address new_limit);
  void UpdateInlineAllocationLimit(size_t min_size) override;
  bool SupportsInlineAllocation() override {
    return identity() == OLD_SPACE && !is_local_space();
  }

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
  bool Expand();

  // Sets up a linear allocation area that fits the given number of bytes.
  // Returns false if there is not enough space and the caller has to retry
  // after collecting garbage.
  inline bool EnsureLinearAllocationArea(int size_in_bytes,
                                         AllocationOrigin origin);
  // Allocates an object from the linear allocation area. Assumes that the
  // linear allocation area is large enought to fit the object.
  inline HeapObject AllocateLinearly(int size_in_bytes);
  // Tries to allocate an aligned object from the linear allocation area.
  // Returns nullptr if the linear allocation area does not fit the object.
  // Otherwise, returns the object pointer and writes the allocation size
  // (object size + alignment filler size) to the size_in_bytes.
  inline HeapObject TryAllocateLinearlyAligned(int* size_in_bytes,
                                               AllocationAlignment alignment);

  V8_WARN_UNUSED_RESULT bool RefillLinearAllocationAreaFromFreeList(
      size_t size_in_bytes, AllocationOrigin origin);

  // If sweeping is still in progress try to sweep unswept pages. If that is
  // not successful, wait for the sweeper threads and retry free-list
  // allocation. Returns false if there is not enough space and the caller
  // has to retry after collecting garbage.
  V8_WARN_UNUSED_RESULT bool EnsureSweptAndRetryAllocation(
      int size_in_bytes, AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT bool SweepAndRetryAllocation(int required_freed_bytes,
                                                     int max_pages,
                                                     int size_in_bytes,
                                                     AllocationOrigin origin);

  // Slow path of AllocateRaw. This function is space-dependent. Returns false
  // if there is not enough space and the caller has to retry after
  // collecting garbage.
  V8_WARN_UNUSED_RESULT virtual bool SlowRefillLinearAllocationArea(
      int size_in_bytes, AllocationOrigin origin);

  // Implementation of SlowAllocateRaw. Returns false if there is not enough
  // space and the caller has to retry after collecting garbage.
  V8_WARN_UNUSED_RESULT bool RawSlowRefillLinearAllocationArea(
      int size_in_bytes, AllocationOrigin origin);

  Executability executable_;

  LocalSpaceKind local_space_kind_;

  size_t area_size_;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  // Mutex guarding any concurrent access to the space.
  base::Mutex space_mutex_;

  friend class IncrementalMarking;
  friend class MarkCompactCollector;

  // Used in cctest.
  friend class heap::HeapTester;
};

enum SemiSpaceId { kFromSpace = 0, kToSpace = 1 };

// -----------------------------------------------------------------------------
// SemiSpace in young generation
//
// A SemiSpace is a contiguous chunk of memory holding page-like memory chunks.
// The mark-compact collector  uses the memory of the first page in the from
// space as a marking stack when tracing live objects.
class SemiSpace : public Space {
 public:
  using iterator = PageIterator;

  static void Swap(SemiSpace* from, SemiSpace* to);

  SemiSpace(Heap* heap, SemiSpaceId semispace)
      : Space(heap, NEW_SPACE, new NoFreeList()),
        current_capacity_(0),
        maximum_capacity_(0),
        minimum_capacity_(0),
        age_mark_(kNullAddress),
        committed_(false),
        id_(semispace),
        current_page_(nullptr),
        pages_used_(0) {}

  inline bool Contains(HeapObject o);
  inline bool Contains(Object o);
  inline bool ContainsSlow(Address a);

  void SetUp(size_t initial_capacity, size_t maximum_capacity);
  void TearDown();

  bool Commit();
  bool Uncommit();
  bool is_committed() { return committed_; }

  // Grow the semispace to the new capacity.  The new capacity requested must
  // be larger than the current capacity and less than the maximum capacity.
  bool GrowTo(size_t new_capacity);

  // Shrinks the semispace to the new capacity.  The new capacity requested
  // must be more than the amount of used memory in the semispace and less
  // than the current capacity.
  bool ShrinkTo(size_t new_capacity);

  bool EnsureCurrentCapacity();

  Address space_end() { return memory_chunk_list_.back()->area_end(); }

  // Returns the start address of the first page of the space.
  Address space_start() {
    DCHECK_NE(memory_chunk_list_.front(), nullptr);
    return memory_chunk_list_.front()->area_start();
  }

  Page* current_page() { return current_page_; }
  int pages_used() { return pages_used_; }

  // Returns the start address of the current page of the space.
  Address page_low() { return current_page_->area_start(); }

  // Returns one past the end address of the current page of the space.
  Address page_high() { return current_page_->area_end(); }

  bool AdvancePage() {
    Page* next_page = current_page_->next_page();
    // We cannot expand if we reached the maximum number of pages already. Note
    // that we need to account for the next page already for this check as we
    // could potentially fill the whole page after advancing.
    const bool reached_max_pages = (pages_used_ + 1) == max_pages();
    if (next_page == nullptr || reached_max_pages) {
      return false;
    }
    current_page_ = next_page;
    pages_used_++;
    return true;
  }

  // Resets the space to using the first page.
  void Reset();

  void RemovePage(Page* page);
  void PrependPage(Page* page);

  Page* InitializePage(MemoryChunk* chunk);

  // Age mark accessors.
  Address age_mark() { return age_mark_; }
  void set_age_mark(Address mark);

  // Returns the current capacity of the semispace.
  size_t current_capacity() { return current_capacity_; }

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

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

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

  inline int max_pages() {
    return static_cast<int>(current_capacity_ / Page::kPageSize);
  }

  // Copies the flags into the masked positions on all pages in the space.
  void FixPagesFlags(intptr_t flags, intptr_t flag_mask);

  // The currently committed space capacity.
  size_t current_capacity_;

  // The maximum capacity that can be used by this space. A space cannot grow
  // beyond that size.
  size_t maximum_capacity_;

  // The minimum capacity for the space. A space cannot shrink below this size.
  size_t minimum_capacity_;

  // Used to govern object promotion during mark-compact collection.
  Address age_mark_;

  bool committed_;
  SemiSpaceId id_;

  Page* current_page_;

  int pages_used_;

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

  NewSpace(Heap* heap, v8::PageAllocator* page_allocator,
           size_t initial_semispace_capacity, size_t max_semispace_capacity);

  ~NewSpace() override { TearDown(); }

  inline bool ContainsSlow(Address a);
  inline bool Contains(Object o);
  inline bool Contains(HeapObject o);

  // Tears down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  void TearDown();

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
    return to_space_.pages_used() *
               MemoryChunkLayout::AllocatableMemoryInDataPage() +
           static_cast<size_t>(top() - to_space_.page_low());
  }

  size_t SizeOfObjects() final { return Size(); }

  // Return the allocatable capacity of a semispace.
  size_t Capacity() {
    SLOW_DCHECK(to_space_.current_capacity() == from_space_.current_capacity());
    return (to_space_.current_capacity() / Page::kPageSize) *
           MemoryChunkLayout::AllocatableMemoryInDataPage();
  }

  // Return the current size of a semispace, allocatable and non-allocatable
  // memory.
  size_t TotalCapacity() {
    DCHECK(to_space_.current_capacity() == from_space_.current_capacity());
    return to_space_.current_capacity();
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

  void ResetOriginalTop() {
    DCHECK_GE(top(), original_top_);
    DCHECK_LE(top(), original_limit_);
    original_top_.store(top(), std::memory_order_release);
  }

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
  AllocateRawAligned(int size_in_bytes, AllocationAlignment alignment,
                     AllocationOrigin origin = AllocationOrigin::kRuntime);

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult AllocateRawUnaligned(
      int size_in_bytes, AllocationOrigin origin = AllocationOrigin::kRuntime);

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

  inline bool ToSpaceContainsSlow(Address a);
  inline bool ToSpaceContains(Object o);
  inline bool FromSpaceContains(Object o);

  // Try to switch the active semispace to a new, empty, page.
  // Returns false if this isn't possible or reasonable (i.e., there
  // are no pages, or the current page is already empty), or true
  // if successful.
  bool AddFreshPage();
  bool AddFreshPageSynchronized();

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
    if (from_space_.is_committed()) return true;
    return from_space_.Commit();
  }

  bool UncommitFromSpace() {
    if (!from_space_.is_committed()) return true;
    return from_space_.Uncommit();
  }

  bool IsFromSpaceCommitted() { return from_space_.is_committed(); }

  SemiSpace* active_space() { return &to_space_; }

  Page* first_page() { return to_space_.first_page(); }
  Page* last_page() { return to_space_.last_page(); }

  iterator begin() { return to_space_.begin(); }
  iterator end() { return to_space_.end(); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

  SemiSpace& from_space() { return from_space_; }
  SemiSpace& to_space() { return to_space_; }

 private:
  // Update linear allocation area to match the current to-space page.
  void UpdateLinearAllocationArea();

  base::Mutex mutex_;

  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks.
  std::atomic<Address> original_top_;
  std::atomic<Address> original_limit_;

  // The semispaces.
  SemiSpace to_space_;
  SemiSpace from_space_;
  VirtualMemory reservation_;

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment);
  bool SupportsInlineAllocation() override { return true; }

  friend class SemiSpaceObjectIterator;
};

class V8_EXPORT_PRIVATE PauseAllocationObserversScope {
 public:
  explicit PauseAllocationObserversScope(Heap* heap);
  ~PauseAllocationObserversScope();

 private:
  Heap* heap_;
  DISALLOW_COPY_AND_ASSIGN(PauseAllocationObserversScope);
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

 protected:
  // The space is temporary and not included in any snapshots.
  bool snapshotable() override { return false; }
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
  V8_WARN_UNUSED_RESULT bool SlowRefillLinearAllocationArea(
      int size_in_bytes, AllocationOrigin origin) override;
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

// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define DCHECK_SEMISPACE_ALLOCATION_INFO(info, space) \
  SLOW_DCHECK((space).page_low() <= (info).top() &&   \
              (info).top() <= (space).page_high() &&  \
              (info).limit() <= (space).page_high())

// -----------------------------------------------------------------------------
// Old space for all map objects

class MapSpace : public PagedSpace {
 public:
  // Creates a map space object.
  explicit MapSpace(Heap* heap)
      : PagedSpace(heap, MAP_SPACE, NOT_EXECUTABLE, new FreeListMap()) {}

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

// -----------------------------------------------------------------------------
// Off-thread space that is used for folded allocation on a different thread.

class V8_EXPORT_PRIVATE OffThreadSpace : public LocalSpace {
 public:
  explicit OffThreadSpace(Heap* heap)
      : LocalSpace(heap, OLD_SPACE, NOT_EXECUTABLE,
                   LocalSpaceKind::kOffThreadSpace) {}

 protected:
  V8_WARN_UNUSED_RESULT bool SlowRefillLinearAllocationArea(
      int size_in_bytes, AllocationOrigin origin) override;

  void RefillFreeList() override;
};

// -----------------------------------------------------------------------------
// Read Only space for all Immortal Immovable and Immutable objects

class ReadOnlySpace : public PagedSpace {
 public:
  explicit ReadOnlySpace(Heap* heap);

  // TODO(v8:7464): Remove this once PagedSpace::Unseal no longer writes to
  // memory_chunk_list_.
  ~ReadOnlySpace() override { Unseal(); }

  bool writable() const { return !is_marked_read_only_; }

  bool Contains(Address a) = delete;
  bool Contains(Object o) = delete;

  V8_EXPORT_PRIVATE void ClearStringPaddingIfNeeded();

  enum class SealMode { kDetachFromHeapAndForget, kDoNotDetachFromHeap };

  // Seal the space by marking it read-only, optionally detaching it
  // from the heap and forgetting it for memory bookkeeping purposes (e.g.
  // prevent space's memory from registering as leaked).
  void Seal(SealMode ro_mode);

  // During boot the free_space_map is created, and afterwards we may need
  // to write it into the free list nodes that were already created.
  void RepairFreeListsAfterDeserialization();

  size_t Available() override { return 0; }

 private:
  // Unseal the space after is has been sealed, by making it writable.
  // TODO(v8:7464): Only possible if the space hasn't been detached.
  void Unseal();
  void SetPermissionsForPages(MemoryAllocator* memory_allocator,
                              PageAllocator::Permission access);

  bool is_marked_read_only_ = false;

  //
  // String padding must be cleared just before serialization and therefore the
  // string padding in the space will already have been cleared if the space was
  // deserialized.
  bool is_string_padding_cleared_;
};

// -----------------------------------------------------------------------------
// Large objects ( > kMaxRegularHeapObjectSize ) are allocated and
// managed by the large object space.
// Large objects do not move during garbage collections.

class V8_EXPORT_PRIVATE LargeObjectSpace : public Space {
 public:
  using iterator = LargePageIterator;

  ~LargeObjectSpace() override { TearDown(); }

  // Releases internal resources, frees objects in this space.
  void TearDown();

  // Available bytes for objects in this space.
  size_t Available() override;

  size_t Size() override { return size_; }
  size_t SizeOfObjects() override { return objects_size_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  int PageCount() { return page_count_; }

  // Frees unmarked objects.
  virtual void FreeUnmarkedObjects();

  // Checks whether a heap object is in this space; O(1).
  bool Contains(HeapObject obj);
  // Checks whether an address is in the object area in this space. Iterates
  // all objects in the space. May be slow.
  bool ContainsSlow(Address addr);

  // Checks whether the space is empty.
  bool IsEmpty() { return first_page() == nullptr; }

  virtual void AddPage(LargePage* page, size_t object_size);
  virtual void RemovePage(LargePage* page, size_t object_size);

  LargePage* first_page() {
    return reinterpret_cast<LargePage*>(Space::first_page());
  }

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

#ifdef VERIFY_HEAP
  virtual void Verify(Isolate* isolate);
#endif

#ifdef DEBUG
  void Print() override;
#endif

 protected:
  LargeObjectSpace(Heap* heap, AllocationSpace id);

  LargePage* AllocateLargePage(int object_size, Executability executable);

  size_t size_;          // allocated bytes
  int page_count_;       // number of chunks
  size_t objects_size_;  // size of objects

 private:
  friend class LargeObjectSpaceObjectIterator;
};

class OffThreadLargeObjectSpace;

class OldLargeObjectSpace : public LargeObjectSpace {
 public:
  explicit OldLargeObjectSpace(Heap* heap);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRaw(int object_size);

  // Clears the marking state of live objects.
  void ClearMarkingStateOfLiveObjects();

  void PromoteNewLargeObject(LargePage* page);

  V8_EXPORT_PRIVATE void MergeOffThreadSpace(OffThreadLargeObjectSpace* other);

 protected:
  explicit OldLargeObjectSpace(Heap* heap, AllocationSpace id);
  V8_WARN_UNUSED_RESULT AllocationResult AllocateRaw(int object_size,
                                                     Executability executable);
};

class NewLargeObjectSpace : public LargeObjectSpace {
 public:
  NewLargeObjectSpace(Heap* heap, size_t capacity);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRaw(int object_size);

  // Available bytes for objects in this space.
  size_t Available() override;

  void Flip();

  void FreeDeadObjects(const std::function<bool(HeapObject)>& is_dead);

  void SetCapacity(size_t capacity);

  // The last allocated object that is not guaranteed to be initialized when
  // the concurrent marker visits it.
  Address pending_object() {
    return pending_object_.load(std::memory_order_relaxed);
  }

  void ResetPendingObject() { pending_object_.store(0); }

 private:
  std::atomic<Address> pending_object_;
  size_t capacity_;
};

class CodeLargeObjectSpace : public OldLargeObjectSpace {
 public:
  explicit CodeLargeObjectSpace(Heap* heap);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRaw(int object_size);

  // Finds a large object page containing the given address, returns nullptr
  // if such a page doesn't exist.
  LargePage* FindPage(Address a);

 protected:
  void AddPage(LargePage* page, size_t object_size) override;
  void RemovePage(LargePage* page, size_t object_size) override;

 private:
  static const size_t kInitialChunkMapCapacity = 1024;
  void InsertChunkMapEntries(LargePage* page);
  void RemoveChunkMapEntries(LargePage* page);

  // Page-aligned addresses to their corresponding LargePage.
  std::unordered_map<Address, LargePage*> chunk_map_;
};

class V8_EXPORT_PRIVATE OffThreadLargeObjectSpace : public LargeObjectSpace {
 public:
  explicit OffThreadLargeObjectSpace(Heap* heap);

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRaw(int object_size);

  void FreeUnmarkedObjects() override;

 protected:
  // OldLargeObjectSpace can mess with OffThreadLargeObjectSpace during merging.
  friend class OldLargeObjectSpace;

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRaw(int object_size,
                                                     Executability executable);
};

class LargeObjectSpaceObjectIterator : public ObjectIterator {
 public:
  explicit LargeObjectSpaceObjectIterator(LargeObjectSpace* space);

  HeapObject Next() override;

 private:
  LargePage* current_;
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

#endif  // V8_HEAP_SPACES_H_
