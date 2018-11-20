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

#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/base/iterator.h"
#include "src/base/list.h"
#include "src/base/platform/mutex.h"
#include "src/cancelable-task.h"
#include "src/flags.h"
#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/heap/invalidated-slots.h"
#include "src/heap/marking.h"
#include "src/objects.h"
#include "src/objects/map.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace heap {
class HeapTester;
class TestCodeRangeScope;
}  // namespace heap

class AllocationObserver;
class CompactionSpace;
class CompactionSpaceCollection;
class FreeList;
class Isolate;
class LinearAllocationArea;
class LocalArrayBufferTracker;
class MemoryAllocator;
class MemoryChunk;
class Page;
class PagedSpace;
class SemiSpace;
class SkipList;
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

#define DCHECK_PAGE_ALIGNED(address) \
  DCHECK((OffsetFrom(address) & Page::kPageAlignmentMask) == 0)

#define DCHECK_OBJECT_ALIGNED(address) \
  DCHECK((OffsetFrom(address) & kObjectAlignmentMask) == 0)

#define DCHECK_OBJECT_SIZE(size) \
  DCHECK((0 < size) && (size <= kMaxRegularHeapObjectSize))

#define DCHECK_CODEOBJECT_SIZE(size, code_space) \
  DCHECK((0 < size) && (size <= code_space->AreaSize()))

#define DCHECK_PAGE_OFFSET(offset) \
  DCHECK((Page::kObjectStartOffset <= offset) && (offset <= Page::kPageSize))

enum FreeListCategoryType {
  kTiniest,
  kTiny,
  kSmall,
  kMedium,
  kLarge,
  kHuge,

  kFirstCategory = kTiniest,
  kLastCategory = kHuge,
  kNumberOfCategories = kLastCategory + 1,
  kInvalidCategory
};

enum FreeMode { kLinkCategory, kDoNotLinkCategory };

enum class SpaceAccountingMode { kSpaceAccounted, kSpaceUnaccounted };

enum ExternalBackingStoreType {
  kArrayBuffer,
  kExternalString,
  kNumTypes
};

enum RememberedSetType {
  OLD_TO_NEW,
  OLD_TO_OLD,
  NUMBER_OF_REMEMBERED_SET_TYPES = OLD_TO_OLD + 1
};

// A free list category maintains a linked list of free memory blocks.
class FreeListCategory {
 public:
  FreeListCategory(FreeList* free_list, Page* page)
      : free_list_(free_list),
        page_(page),
        type_(kInvalidCategory),
        available_(0),
        top_(nullptr),
        prev_(nullptr),
        next_(nullptr) {}

  void Initialize(FreeListCategoryType type) {
    type_ = type;
    available_ = 0;
    top_ = nullptr;
    prev_ = nullptr;
    next_ = nullptr;
  }

  void Reset();

  void ResetStats() { Reset(); }

  void RepairFreeList(Heap* heap);

  // Relinks the category into the currently owning free list. Requires that the
  // category is currently unlinked.
  void Relink();

  void Free(Address address, size_t size_in_bytes, FreeMode mode);

  // Performs a single try to pick a node of at least |minimum_size| from the
  // category. Stores the actual size in |node_size|. Returns nullptr if no
  // node is found.
  FreeSpace* PickNodeFromList(size_t minimum_size, size_t* node_size);

  // Picks a node of at least |minimum_size| from the category. Stores the
  // actual size in |node_size|. Returns nullptr if no node is found.
  FreeSpace* SearchForNodeInList(size_t minimum_size, size_t* node_size);

  inline FreeList* owner();
  inline Page* page() const { return page_; }
  inline bool is_linked();
  bool is_empty() { return top() == nullptr; }
  size_t available() const { return available_; }

  void set_free_list(FreeList* free_list) { free_list_ = free_list; }

#ifdef DEBUG
  size_t SumFreeList();
  int FreeListLength();
#endif

 private:
  // For debug builds we accurately compute free lists lengths up until
  // {kVeryLongFreeList} by manually walking the list.
  static const int kVeryLongFreeList = 500;

  FreeSpace* top() { return top_; }
  void set_top(FreeSpace* top) { top_ = top; }
  FreeListCategory* prev() { return prev_; }
  void set_prev(FreeListCategory* prev) { prev_ = prev; }
  FreeListCategory* next() { return next_; }
  void set_next(FreeListCategory* next) { next_ = next; }

  // This FreeListCategory is owned by the given free_list_.
  FreeList* free_list_;

  // This FreeListCategory holds free list entries of the given page_.
  Page* const page_;

  // |type_|: The type of this free list category.
  FreeListCategoryType type_;

  // |available_|: Total available bytes in all blocks of this free list
  // category.
  size_t available_;

  // |top_|: Points to the top FreeSpace* in the free list category.
  FreeSpace* top_;

  FreeListCategory* prev_;
  FreeListCategory* next_;

  friend class FreeList;
  friend class PagedSpace;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FreeListCategory);
};

// MemoryChunk represents a memory region owned by a specific space.
// It is divided into the header and the body. Chunk start is always
// 1MB aligned. Start of the body is aligned so it can accommodate
// any heap object.
class MemoryChunk {
 public:
  // Use with std data structures.
  struct Hasher {
    size_t operator()(MemoryChunk* const chunk) const {
      return reinterpret_cast<size_t>(chunk) >> kPageSizeBits;
    }
  };

  enum Flag {
    NO_FLAGS = 0u,
    IS_EXECUTABLE = 1u << 0,
    POINTERS_TO_HERE_ARE_INTERESTING = 1u << 1,
    POINTERS_FROM_HERE_ARE_INTERESTING = 1u << 2,
    // A page in new space has one of the next two flags set.
    IN_FROM_SPACE = 1u << 3,
    IN_TO_SPACE = 1u << 4,
    NEW_SPACE_BELOW_AGE_MARK = 1u << 5,
    EVACUATION_CANDIDATE = 1u << 6,
    NEVER_EVACUATE = 1u << 7,

    // Large objects can have a progress bar in their page header. These object
    // are scanned in increments and will be kept black while being scanned.
    // Even if the mutator writes to them they will be kept black and a white
    // to grey transition is performed in the value.
    HAS_PROGRESS_BAR = 1u << 8,

    // |PAGE_NEW_OLD_PROMOTION|: A page tagged with this flag has been promoted
    // from new to old space during evacuation.
    PAGE_NEW_OLD_PROMOTION = 1u << 9,

    // |PAGE_NEW_NEW_PROMOTION|: A page tagged with this flag has been moved
    // within the new space during evacuation.
    PAGE_NEW_NEW_PROMOTION = 1u << 10,

    // This flag is intended to be used for testing. Works only when both
    // FLAG_stress_compaction and FLAG_manual_evacuation_candidates_selection
    // are set. It forces the page to become an evacuation candidate at next
    // candidates selection cycle.
    FORCE_EVACUATION_CANDIDATE_FOR_TESTING = 1u << 11,

    // This flag is intended to be used for testing.
    NEVER_ALLOCATE_ON_PAGE = 1u << 12,

    // The memory chunk is already logically freed, however the actual freeing
    // still has to be performed.
    PRE_FREED = 1u << 13,

    // |POOLED|: When actually freeing this chunk, only uncommit and do not
    // give up the reservation as we still reuse the chunk at some point.
    POOLED = 1u << 14,

    // |COMPACTION_WAS_ABORTED|: Indicates that the compaction in this page
    //   has been aborted and needs special handling by the sweeper.
    COMPACTION_WAS_ABORTED = 1u << 15,

    // |COMPACTION_WAS_ABORTED_FOR_TESTING|: During stress testing evacuation
    // on pages is sometimes aborted. The flag is used to avoid repeatedly
    // triggering on the same page.
    COMPACTION_WAS_ABORTED_FOR_TESTING = 1u << 16,

    // |SWEEP_TO_ITERATE|: The page requires sweeping using external markbits
    // to iterate the page.
    SWEEP_TO_ITERATE = 1u << 17
  };

  using Flags = uintptr_t;

  static const Flags kPointersToHereAreInterestingMask =
      POINTERS_TO_HERE_ARE_INTERESTING;

  static const Flags kPointersFromHereAreInterestingMask =
      POINTERS_FROM_HERE_ARE_INTERESTING;

  static const Flags kEvacuationCandidateMask = EVACUATION_CANDIDATE;

  static const Flags kIsInNewSpaceMask = IN_FROM_SPACE | IN_TO_SPACE;

  static const Flags kSkipEvacuationSlotsRecordingMask =
      kEvacuationCandidateMask | kIsInNewSpaceMask;

  // |kSweepingDone|: The page state when sweeping is complete or sweeping must
  //   not be performed on that page. Sweeper threads that are done with their
  //   work will set this value and not touch the page anymore.
  // |kSweepingPending|: This page is ready for parallel sweeping.
  // |kSweepingInProgress|: This page is currently swept by a sweeper thread.
  enum ConcurrentSweepingState {
    kSweepingDone,
    kSweepingPending,
    kSweepingInProgress,
  };

  static const intptr_t kAlignment =
      (static_cast<uintptr_t>(1) << kPageSizeBits);

  static const intptr_t kAlignmentMask = kAlignment - 1;

  static const intptr_t kSizeOffset = 0;
  static const intptr_t kFlagsOffset = kSizeOffset + kSizetSize;
  static const intptr_t kAreaStartOffset = kFlagsOffset + kIntptrSize;
  static const intptr_t kAreaEndOffset = kAreaStartOffset + kPointerSize;
  static const intptr_t kReservationOffset = kAreaEndOffset + kPointerSize;
  static const intptr_t kOwnerOffset = kReservationOffset + 2 * kPointerSize;

  static const size_t kMinHeaderSize =
      kSizeOffset         // NOLINT
      + kSizetSize        // size_t size
      + kUIntptrSize      // uintptr_t flags_
      + kPointerSize      // Address area_start_
      + kPointerSize      // Address area_end_
      + 2 * kPointerSize  // VirtualMemory reservation_
      + kPointerSize      // Address owner_
      + kPointerSize      // Heap* heap_
      + kIntptrSize       // intptr_t progress_bar_
      + kIntptrSize       // std::atomic<intptr_t> live_byte_count_
      + kPointerSize * NUMBER_OF_REMEMBERED_SET_TYPES  // SlotSet* array
      + kPointerSize * NUMBER_OF_REMEMBERED_SET_TYPES  // TypedSlotSet* array
      + kPointerSize  // InvalidatedSlots* invalidated_slots_
      + kPointerSize  // SkipList* skip_list_
      + kPointerSize  // std::atomic<intptr_t> high_water_mark_
      + kPointerSize  // base::Mutex* mutex_
      +
      kPointerSize  // std::atomic<ConcurrentSweepingState> concurrent_sweeping_
      + kPointerSize  // base::Mutex* page_protection_change_mutex_
      + kPointerSize  // unitptr_t write_unprotect_counter_
      + kSizetSize * kNumTypes
      // std::atomic<size_t> external_backing_store_bytes_
      + kSizetSize        // size_t allocated_bytes_
      + kSizetSize        // size_t wasted_memory_
      + kPointerSize * 2  // base::ListNode
      + kPointerSize * kNumberOfCategories
      // FreeListCategory categories_[kNumberOfCategories]
      + kPointerSize  // LocalArrayBufferTracker* local_tracker_
      + kIntptrSize   // std::atomic<intptr_t> young_generation_live_byte_count_
      + kPointerSize;  // Bitmap* young_generation_bitmap_

  // We add some more space to the computed header size to amount for missing
  // alignment requirements in our computation.
  // Try to get kHeaderSize properly aligned on 32-bit and 64-bit machines.
  static const size_t kHeaderSize = kMinHeaderSize;

  static const int kBodyOffset =
      CODE_POINTER_ALIGN(kHeaderSize + Bitmap::kSize);

  // The start offset of the object area in a page. Aligned to both maps and
  // code alignment to be suitable for both.  Also aligned to 32 words because
  // the marking bitmap is arranged in 32 bit chunks.
  static const int kObjectStartAlignment = 32 * kPointerSize;
  static const int kObjectStartOffset =
      kBodyOffset - 1 +
      (kObjectStartAlignment - (kBodyOffset - 1) % kObjectStartAlignment);

  // Page size in bytes.  This must be a multiple of the OS page size.
  static const int kPageSize = 1 << kPageSizeBits;
  static const intptr_t kPageAlignmentMask = (1 << kPageSizeBits) - 1;

  static const int kAllocatableMemory = kPageSize - kObjectStartOffset;

  // Maximum number of nested code memory modification scopes.
  // TODO(6792,mstarzinger): Drop to 3 or lower once WebAssembly is off heap.
  static const int kMaxWriteUnprotectCounter = 4;

  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromAddress(Address a) {
    return reinterpret_cast<MemoryChunk*>(OffsetFrom(a) & ~kAlignmentMask);
  }
  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromHeapObject(const HeapObject* o) {
    return reinterpret_cast<MemoryChunk*>(reinterpret_cast<Address>(o) &
                                          ~kAlignmentMask);
  }

  void SetOldGenerationPageFlags(bool is_marking);
  void SetYoungGenerationPageFlags(bool is_marking);

  static inline MemoryChunk* FromAnyPointerAddress(Heap* heap, Address addr);

  static inline void UpdateHighWaterMark(Address mark) {
    if (mark == kNullAddress) return;
    // Need to subtract one from the mark because when a chunk is full the
    // top points to the next address after the chunk, which effectively belongs
    // to another chunk. See the comment to Page::FromTopOrLimit.
    MemoryChunk* chunk = MemoryChunk::FromAddress(mark - 1);
    intptr_t new_mark = static_cast<intptr_t>(mark - chunk->address());
    intptr_t old_mark = 0;
    do {
      old_mark = chunk->high_water_mark_;
    } while (
        (new_mark > old_mark) &&
        !chunk->high_water_mark_.compare_exchange_weak(old_mark, new_mark));
  }

  Address address() const {
    return reinterpret_cast<Address>(const_cast<MemoryChunk*>(this));
  }

  base::Mutex* mutex() { return mutex_; }

  bool Contains(Address addr) {
    return addr >= area_start() && addr < area_end();
  }

  // Checks whether |addr| can be a limit of addresses in this page. It's a
  // limit if it's in the page, or if it's just after the last byte of the page.
  bool ContainsLimit(Address addr) {
    return addr >= area_start() && addr <= area_end();
  }

  void set_concurrent_sweeping_state(ConcurrentSweepingState state) {
    concurrent_sweeping_ = state;
  }

  ConcurrentSweepingState concurrent_sweeping_state() {
    return static_cast<ConcurrentSweepingState>(concurrent_sweeping_.load());
  }

  bool SweepingDone() { return concurrent_sweeping_ == kSweepingDone; }

  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }

  inline Heap* heap() const { return heap_; }

  Heap* synchronized_heap();

  inline SkipList* skip_list() { return skip_list_; }

  inline void set_skip_list(SkipList* skip_list) { skip_list_ = skip_list; }

  template <RememberedSetType type>
  bool ContainsSlots() {
    return slot_set<type>() != nullptr || typed_slot_set<type>() != nullptr ||
           invalidated_slots() != nullptr;
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  SlotSet* slot_set() {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&slot_set_[type]);
    return slot_set_[type];
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  TypedSlotSet* typed_slot_set() {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&typed_slot_set_[type]);
    return typed_slot_set_[type];
  }

  template <RememberedSetType type>
  SlotSet* AllocateSlotSet();
  // Not safe to be called concurrently.
  template <RememberedSetType type>
  void ReleaseSlotSet();
  template <RememberedSetType type>
  TypedSlotSet* AllocateTypedSlotSet();
  // Not safe to be called concurrently.
  template <RememberedSetType type>
  void ReleaseTypedSlotSet();

  InvalidatedSlots* AllocateInvalidatedSlots();
  void ReleaseInvalidatedSlots();
  void RegisterObjectWithInvalidatedSlots(HeapObject* object, int size);
  InvalidatedSlots* invalidated_slots() { return invalidated_slots_; }

  void ReleaseLocalTracker();

  void AllocateYoungGenerationBitmap();
  void ReleaseYoungGenerationBitmap();

  Address area_start() { return area_start_; }
  Address area_end() { return area_end_; }
  size_t area_size() { return static_cast<size_t>(area_end() - area_start()); }

  // Approximate amount of physical memory committed for this chunk.
  size_t CommittedPhysicalMemory();

  Address HighWaterMark() { return address() + high_water_mark_; }

  int progress_bar() {
    DCHECK(IsFlagSet(HAS_PROGRESS_BAR));
    return static_cast<int>(progress_bar_);
  }

  void set_progress_bar(int progress_bar) {
    DCHECK(IsFlagSet(HAS_PROGRESS_BAR));
    progress_bar_ = progress_bar;
  }

  void ResetProgressBar() {
    if (IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
      set_progress_bar(0);
    }
  }

  void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                          size_t amount);
  void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                          size_t amount);
  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) {
    return external_backing_store_bytes_[type];
  }

  inline uint32_t AddressToMarkbitIndex(Address addr) const {
    return static_cast<uint32_t>(addr - this->address()) >> kPointerSizeLog2;
  }

  inline Address MarkbitIndexToAddress(uint32_t index) const {
    return this->address() + (index << kPointerSizeLog2);
  }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  void SetFlag(Flag flag) {
    if (access_mode == AccessMode::NON_ATOMIC) {
      flags_ |= flag;
    } else {
      base::AsAtomicWord::SetBits<uintptr_t>(&flags_, flag, flag);
    }
  }

  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  bool IsFlagSet(Flag flag) {
    return (GetFlags<access_mode>() & flag) != 0;
  }

  void ClearFlag(Flag flag) { flags_ &= ~flag; }
  // Set or clear multiple flags at a time. The flags in the mask are set to
  // the value in "flags", the rest retain the current value in |flags_|.
  void SetFlags(uintptr_t flags, uintptr_t mask) {
    flags_ = (flags_ & ~mask) | (flags & mask);
  }

  // Return all current flags.
  template <AccessMode access_mode = AccessMode::NON_ATOMIC>
  uintptr_t GetFlags() {
    if (access_mode == AccessMode::NON_ATOMIC) {
      return flags_;
    } else {
      return base::AsAtomicWord::Relaxed_Load(&flags_);
    }
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

  bool InNewSpace() { return (flags_ & kIsInNewSpaceMask) != 0; }

  bool InToSpace() { return IsFlagSet(IN_TO_SPACE); }

  bool InFromSpace() { return IsFlagSet(IN_FROM_SPACE); }

  Space* owner() const { return owner_; }

  void set_owner(Space* space) { owner_ = space; }

  bool IsPagedSpace() const;

  // Emits a memory barrier. For TSAN builds the other thread needs to perform
  // MemoryChunk::synchronized_heap() to simulate the barrier.
  void InitializationMemoryFence();

  void SetReadAndExecutable();
  void SetReadAndWritable();

  base::ListNode<MemoryChunk>& list_node() { return list_node_; }

 protected:
  static MemoryChunk* Initialize(Heap* heap, Address base, size_t size,
                                 Address area_start, Address area_end,
                                 Executability executable, Space* owner,
                                 VirtualMemory* reservation);

  // Should be called when memory chunk is about to be freed.
  void ReleaseAllocatedMemory();

  VirtualMemory* reserved_memory() { return &reservation_; }

  size_t size_;
  uintptr_t flags_;

  // Start and end of allocatable memory on this chunk.
  Address area_start_;
  Address area_end_;

  // If the chunk needs to remember its memory reservation, it is stored here.
  VirtualMemory reservation_;

  // The space owning this memory chunk.
  std::atomic<Space*> owner_;

  Heap* heap_;

  // Used by the incremental marker to keep track of the scanning progress in
  // large objects that have a progress bar and are scanned in increments.
  intptr_t progress_bar_;

  // Count of bytes marked black on page.
  std::atomic<intptr_t> live_byte_count_;

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES];
  TypedSlotSet* typed_slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES];
  InvalidatedSlots* invalidated_slots_;

  SkipList* skip_list_;

  // Assuming the initial allocation on a page is sequential,
  // count highest number of bytes ever allocated on the page.
  std::atomic<intptr_t> high_water_mark_;

  base::Mutex* mutex_;

  std::atomic<intptr_t> concurrent_sweeping_;

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

  FreeListCategory* categories_[kNumberOfCategories];

  LocalArrayBufferTracker* local_tracker_;

  std::atomic<intptr_t> young_generation_live_byte_count_;
  Bitmap* young_generation_bitmap_;

 private:
  void InitializeReservedMemory() { reservation_.Reset(); }

  friend class ConcurrentMarkingState;
  friend class IncrementalMarkingState;
  friend class MajorAtomicMarkingState;
  friend class MajorMarkingState;
  friend class MajorNonAtomicMarkingState;
  friend class MemoryAllocator;
  friend class MemoryChunkValidator;
  friend class MinorMarkingState;
  friend class MinorNonAtomicMarkingState;
  friend class PagedSpace;
};

static_assert(sizeof(std::atomic<intptr_t>) == kPointerSize,
              "sizeof(std::atomic<intptr_t>) == kPointerSize");

static_assert(kMaxRegularHeapObjectSize <= MemoryChunk::kAllocatableMemory,
              "kMaxRegularHeapObjectSize <= MemoryChunk::kAllocatableMemory");


// -----------------------------------------------------------------------------
// A page is a memory chunk of a size 512K. Large object pages may be larger.
//
// The only way to get a page pointer is by calling factory methods:
//   Page* p = Page::FromAddress(addr); or
//   Page* p = Page::FromTopOrLimit(top);
class Page : public MemoryChunk {
 public:
  static const intptr_t kCopyAllFlags = ~0;

  // Page flags copied from from-space to to-space when flipping semispaces.
  static const intptr_t kCopyOnFlipFlagsMask =
      static_cast<intptr_t>(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING) |
      static_cast<intptr_t>(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);

  // Returns the page containing a given address. The address ranges
  // from [page_addr .. page_addr + kPageSize[. This only works if the object
  // is in fact in a page.
  static Page* FromAddress(Address addr) {
    return reinterpret_cast<Page*>(OffsetFrom(addr) & ~kPageAlignmentMask);
  }

  // Returns the page containing the address provided. The address can
  // potentially point righter after the page. To be also safe for tagged values
  // we subtract a hole word. The valid address ranges from
  // [page_addr + kObjectStartOffset .. page_addr + kPageSize + kPointerSize].
  static Page* FromAllocationAreaAddress(Address address) {
    return Page::FromAddress(address - kPointerSize);
  }

  // Checks if address1 and address2 are on the same new space page.
  static bool OnSamePage(Address address1, Address address2) {
    return Page::FromAddress(address1) == Page::FromAddress(address2);
  }

  // Checks whether an address is page aligned.
  static bool IsAlignedToPageSize(Address addr) {
    return (OffsetFrom(addr) & kPageAlignmentMask) == 0;
  }

  static bool IsAtObjectStart(Address addr) {
    return (addr & kPageAlignmentMask) == kObjectStartOffset;
  }

  static Page* ConvertNewToOld(Page* old_page);

  inline void MarkNeverAllocateForTesting();
  inline void MarkEvacuationCandidate();
  inline void ClearEvacuationCandidate();

  Page* next_page() { return static_cast<Page*>(list_node_.next()); }
  Page* prev_page() { return static_cast<Page*>(list_node_.prev()); }

  template <typename Callback>
  inline void ForAllFreeListCategories(Callback callback) {
    for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
      callback(categories_[i]);
    }
  }

  // Returns the offset of a given address to this page.
  inline size_t Offset(Address a) { return static_cast<size_t>(a - address()); }

  // Returns the address for a given offset to the this page.
  Address OffsetToAddress(size_t offset) {
    DCHECK_PAGE_OFFSET(offset);
    return address() + offset;
  }

  // WaitUntilSweepingCompleted only works when concurrent sweeping is in
  // progress. In particular, when we know that right before this call a
  // sweeper thread was sweeping this page.
  void WaitUntilSweepingCompleted() {
    mutex_->Lock();
    mutex_->Unlock();
    DCHECK(SweepingDone());
  }

  void AllocateLocalTracker();
  inline LocalArrayBufferTracker* local_tracker() { return local_tracker_; }
  bool contains_array_buffers();

  void ResetFreeListStatistics();

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

  void ResetAllocatedBytes();

  size_t ShrinkToHighWaterMark();

  V8_EXPORT_PRIVATE void CreateBlackArea(Address start, Address end);
  void DestroyBlackArea(Address start, Address end);

  void InitializeFreeListCategories();
  void AllocateFreeListCategories();
  void ReleaseFreeListCategories();

#ifdef DEBUG
  void Print();
#endif  // DEBUG

 private:
  enum InitializationMode { kFreeMemory, kDoNotFreeMemory };

  friend class MemoryAllocator;
};

class ReadOnlyPage : public Page {
 public:
  // Clears any pointers in the header that point out of the page that would
  // otherwise make the header non-relocatable.
  void MakeHeaderRelocatable();
};

class LargePage : public MemoryChunk {
 public:
  HeapObject* GetObject() { return HeapObject::FromAddress(area_start()); }

  inline LargePage* next_page() {
    return static_cast<LargePage*>(list_node_.next());
  }

  // Uncommit memory that is not in use anymore by the object. If the object
  // cannot be shrunk 0 is returned.
  Address GetAddressToShrink(Address object_address, size_t object_size);

  void ClearOutOfLiveRangeSlots(Address free_start);

  // A limit to guarantee that we do not overflow typed slot offset in
  // the old to old remembered set.
  // Note that this limit is higher than what assembler already imposes on
  // x64 and ia32 architectures.
  static const int kMaxCodePageSize = 512 * MB;

 private:
  static LargePage* Initialize(Heap* heap, MemoryChunk* chunk,
                               Executability executable);

  friend class MemoryAllocator;
};


// ----------------------------------------------------------------------------
// Space is the abstract superclass for all allocation spaces.
class Space : public Malloced {
 public:
  Space(Heap* heap, AllocationSpace id)
      : allocation_observers_paused_(false),
        heap_(heap),
        id_(id),
        committed_(0),
        max_committed_(0) {
    external_backing_store_bytes_ =
        new std::atomic<size_t>[ExternalBackingStoreType::kNumTypes];
    external_backing_store_bytes_[ExternalBackingStoreType::kArrayBuffer] = 0;
    external_backing_store_bytes_[ExternalBackingStoreType::kExternalString] =
        0;
  }

  virtual ~Space() {
    delete[] external_backing_store_bytes_;
    external_backing_store_bytes_ = nullptr;
  }

  Heap* heap() const { return heap_; }

  // Identity used in error reporting.
  AllocationSpace identity() { return id_; }

  const char* name() { return AllocationSpaceName(id_); }

  V8_EXPORT_PRIVATE virtual void AddAllocationObserver(
      AllocationObserver* observer);

  V8_EXPORT_PRIVATE virtual void RemoveAllocationObserver(
      AllocationObserver* observer);

  V8_EXPORT_PRIVATE virtual void PauseAllocationObservers();

  V8_EXPORT_PRIVATE virtual void ResumeAllocationObservers();

  V8_EXPORT_PRIVATE virtual void StartNextInlineAllocationStep() {}

  void AllocationStep(int bytes_since_last, Address soon_object, int size);

  // Return the total amount committed memory for this space, i.e., allocatable
  // memory and page headers.
  virtual size_t CommittedMemory() { return committed_; }

  virtual size_t MaximumCommittedMemory() { return max_committed_; }

  // Returns allocated size.
  virtual size_t Size() = 0;

  // Returns size of objects. Can differ from the allocated size
  // (e.g. see LargeObjectSpace).
  virtual size_t SizeOfObjects() { return Size(); }

  // Returns amount of off-heap memory in-use by objects in this Space.
  virtual size_t ExternalBackingStoreBytes(
      ExternalBackingStoreType type) const {
    return external_backing_store_bytes_[type];
  }

  // Approximate amount of physical memory committed for this space.
  virtual size_t CommittedPhysicalMemory() = 0;

  // Return the available bytes without growing.
  virtual size_t Available() = 0;

  virtual int RoundSizeDownToObjectAlignment(int size) {
    if (id_ == CODE_SPACE) {
      return RoundDown(size, kCodeAlignment);
    } else {
      return RoundDown(size, kPointerSize);
    }
  }

  virtual std::unique_ptr<ObjectIterator> GetObjectIterator() = 0;

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

  void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                          size_t amount) {
    external_backing_store_bytes_[type] += amount;
  }
  void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                          size_t amount) {
    DCHECK_GE(external_backing_store_bytes_[type], amount);
    external_backing_store_bytes_[type] -= amount;
  }

  V8_EXPORT_PRIVATE void* GetRandomMmapAddr();

  MemoryChunk* first_page() { return memory_chunk_list_.front(); }
  MemoryChunk* last_page() { return memory_chunk_list_.back(); }

  base::List<MemoryChunk>& memory_chunk_list() { return memory_chunk_list_; }

#ifdef DEBUG
  virtual void Print() = 0;
#endif

 protected:
  intptr_t GetNextInlineAllocationStepSize();
  bool AllocationObserversActive() {
    return !allocation_observers_paused_ && !allocation_observers_.empty();
  }

  std::vector<AllocationObserver*> allocation_observers_;

  // The List manages the pages that belong to the given space.
  base::List<MemoryChunk> memory_chunk_list_;

  // Tracks off-heap memory used by this space.
  std::atomic<size_t>* external_backing_store_bytes_;

 private:
  bool allocation_observers_paused_;
  Heap* heap_;
  AllocationSpace id_;

  // Keeps track of committed memory in a space.
  size_t committed_;
  size_t max_committed_;

  DISALLOW_COPY_AND_ASSIGN(Space);
};


class MemoryChunkValidator {
  // Computed offsets should match the compiler generated ones.
  STATIC_ASSERT(MemoryChunk::kSizeOffset == offsetof(MemoryChunk, size_));

  // Validate our estimates on the header size.
  STATIC_ASSERT(sizeof(MemoryChunk) <= MemoryChunk::kHeaderSize);
  STATIC_ASSERT(sizeof(LargePage) <= MemoryChunk::kHeaderSize);
  STATIC_ASSERT(sizeof(Page) <= MemoryChunk::kHeaderSize);
};


// ----------------------------------------------------------------------------
// All heap objects containing executable code (code objects) must be allocated
// from a 2 GB range of memory, so that they can call each other using 32-bit
// displacements.  This happens automatically on 32-bit platforms, where 32-bit
// displacements cover the entire 4GB virtual address space.  On 64-bit
// platforms, we support this using the CodeRange object, which reserves and
// manages a range of virtual memory.
class CodeRange {
 public:
  CodeRange(Isolate* isolate, size_t requested_size);
  ~CodeRange();

  bool valid() { return virtual_memory_.IsReserved(); }
  Address start() {
    DCHECK(valid());
    return virtual_memory_.address();
  }
  size_t size() {
    DCHECK(valid());
    return virtual_memory_.size();
  }
  bool contains(Address address) {
    if (!valid()) return false;
    Address start = virtual_memory_.address();
    return start <= address && address < start + virtual_memory_.size();
  }

  // Allocates a chunk of memory from the large-object portion of
  // the code range.  On platforms with no separate code range, should
  // not be called.
  V8_WARN_UNUSED_RESULT Address AllocateRawMemory(const size_t requested_size,
                                                  const size_t commit_size,
                                                  size_t* allocated);
  void FreeRawMemory(Address buf, size_t length);

 private:
  class FreeBlock {
   public:
    FreeBlock() : start(0), size(0) {}
    FreeBlock(Address start_arg, size_t size_arg)
        : start(start_arg), size(size_arg) {
      DCHECK(IsAddressAligned(start, MemoryChunk::kAlignment));
      DCHECK(size >= static_cast<size_t>(Page::kPageSize));
    }
    FreeBlock(void* start_arg, size_t size_arg)
        : start(reinterpret_cast<Address>(start_arg)), size(size_arg) {
      DCHECK(IsAddressAligned(start, MemoryChunk::kAlignment));
      DCHECK(size >= static_cast<size_t>(Page::kPageSize));
    }

    Address start;
    size_t size;
  };

  // Finds a block on the allocation list that contains at least the
  // requested amount of memory.  If none is found, sorts and merges
  // the existing free memory blocks, and searches again.
  // If none can be found, returns false.
  bool GetNextAllocationBlock(size_t requested);
  // Compares the start addresses of two free blocks.
  static bool CompareFreeBlockAddress(const FreeBlock& left,
                                      const FreeBlock& right);
  bool ReserveBlock(const size_t requested_size, FreeBlock* block);
  void ReleaseBlock(const FreeBlock* block);

  Isolate* isolate_;

  // The reserved range of virtual memory that all code objects are put in.
  VirtualMemory virtual_memory_;

  // The global mutex guards free_list_ and allocation_list_ as GC threads may
  // access both lists concurrently to the main thread.
  base::Mutex code_range_mutex_;

  // Freed blocks of memory are added to the free list.  When the allocation
  // list is exhausted, the free list is sorted and merged to make the new
  // allocation list.
  std::vector<FreeBlock> free_list_;

  // Memory is allocated from the free blocks on the allocation list.
  // The block at current_allocation_block_index_ is the current block.
  std::vector<FreeBlock> allocation_list_;
  size_t current_allocation_block_index_;
  size_t requested_code_range_size_;

  DISALLOW_COPY_AND_ASSIGN(CodeRange);
};

// The process-wide singleton that keeps track of code range regions with the
// intention to reuse free code range regions as a workaround for CFG memory
// leaks (see crbug.com/870054).
class CodeRangeAddressHint {
 public:
  // Returns the most recently freed code range start address for the given
  // size. If there is no such entry, then a random address is returned.
  V8_EXPORT_PRIVATE void* GetAddressHint(size_t code_range_size);

  V8_EXPORT_PRIVATE void NotifyFreedCodeRange(void* code_range_start,
                                              size_t code_range_size);

 private:
  base::Mutex mutex_;
  // A map from code range size to an array of recently freed code range
  // addresses. There should be O(1) different code range sizes.
  // The length of each array is limited by the peak number of code ranges,
  // which should be also O(1).
  std::map<size_t, std::vector<void*>> recently_freed_;
};

class SkipList {
 public:
  SkipList() { Clear(); }

  void Clear() {
    for (int idx = 0; idx < kSize; idx++) {
      starts_[idx] = static_cast<Address>(-1);
    }
  }

  Address StartFor(Address addr) { return starts_[RegionNumber(addr)]; }

  void AddObject(Address addr, int size) {
    int start_region = RegionNumber(addr);
    int end_region = RegionNumber(addr + size - kPointerSize);
    for (int idx = start_region; idx <= end_region; idx++) {
      if (starts_[idx] > addr) {
        starts_[idx] = addr;
      } else {
        // In the first region, there may already be an object closer to the
        // start of the region. Do not change the start in that case. If this
        // is not the first region, you probably added overlapping objects.
        DCHECK_EQ(start_region, idx);
      }
    }
  }

  static inline int RegionNumber(Address addr) {
    return (OffsetFrom(addr) & Page::kPageAlignmentMask) >> kRegionSizeLog2;
  }

  static void Update(Address addr, int size) {
    Page* page = Page::FromAddress(addr);
    SkipList* list = page->skip_list();
    if (list == nullptr) {
      list = new SkipList();
      page->set_skip_list(list);
    }

    list->AddObject(addr, size);
  }

 private:
  static const int kRegionSizeLog2 = 13;
  static const int kRegionSize = 1 << kRegionSizeLog2;
  static const int kSize = Page::kPageSize / kRegionSize;

  STATIC_ASSERT(Page::kPageSize % kRegionSize == 0);

  Address starts_[kSize];
};


// ----------------------------------------------------------------------------
// A space acquires chunks of memory from the operating system. The memory
// allocator allocates and deallocates pages for the paged heap spaces and large
// pages for large object space.
class V8_EXPORT_PRIVATE MemoryAllocator {
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
      if (chunk->IsPagedSpace() && chunk->executable() != EXECUTABLE) {
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
          chunk->ReleaseAllocatedMemory();
        }
      }
      return chunk;
    }

    void FreeQueuedChunks();
    void CancelAndWaitForPendingTasks();
    void PrepareForMarkCompact();
    void EnsureUnmappingCompleted();
    void TearDown();
    int NumberOfChunks();
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
      base::LockGuard<base::Mutex> guard(&mutex_);
      chunks_[type].push_back(chunk);
    }

    template <ChunkQueueType type>
    MemoryChunk* GetMemoryChunkSafe() {
      base::LockGuard<base::Mutex> guard(&mutex_);
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

  static size_t CodePageGuardStartOffset();

  static size_t CodePageGuardSize();

  static size_t CodePageAreaStartOffset();

  static size_t CodePageAreaEndOffset();

  static size_t CodePageAreaSize() {
    return CodePageAreaEndOffset() - CodePageAreaStartOffset();
  }

  static size_t PageAreaSize(AllocationSpace space) {
    DCHECK_NE(LO_SPACE, space);
    return (space == CODE_SPACE) ? CodePageAreaSize()
                                 : Page::kAllocatableMemory;
  }

  static intptr_t GetCommitPageSize();

  MemoryAllocator(Isolate* isolate, size_t max_capacity,
                  size_t code_range_size);

  void TearDown();

  // Allocates a Page from the allocator. AllocationMode is used to indicate
  // whether pooled allocation, which only works for MemoryChunk::kPageSize,
  // should be tried first.
  template <MemoryAllocator::AllocationMode alloc_mode = kRegular,
            typename SpaceType>
  Page* AllocatePage(size_t size, SpaceType* owner, Executability executable);

  LargePage* AllocateLargePage(size_t size, LargeObjectSpace* owner,
                               Executability executable);

  template <MemoryAllocator::FreeMode mode = kFull>
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

  // Returns maximum available bytes that the old space can have.
  size_t MaxAvailable() {
    return (Available() / Page::kPageSize) * Page::kAllocatableMemory;
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
  MemoryChunk* AllocateChunk(size_t reserve_area_size, size_t commit_area_size,
                             Executability executable, Space* space);

  Address ReserveAlignedMemory(size_t requested, size_t alignment, void* hint,
                               VirtualMemory* controller);
  Address AllocateAlignedMemory(size_t reserve_size, size_t commit_size,
                                size_t alignment, Executability executable,
                                void* hint, VirtualMemory* controller);

  bool CommitMemory(Address addr, size_t size);

  void FreeMemory(VirtualMemory* reservation, Executability executable);
  void FreeMemory(Address addr, size_t size, Executability executable);

  // Partially release |bytes_to_free| bytes starting at |start_free|. Note that
  // internally memory is freed from |start_free| to the end of the reservation.
  // Additional memory beyond the page is not accounted though, so
  // |bytes_to_free| is computed by the caller.
  void PartialFreeMemory(MemoryChunk* chunk, Address start_free,
                         size_t bytes_to_free, Address new_area_end);

  // Commit a contiguous block of memory from the initial chunk.  Assumes that
  // the address is not kNullAddress, the size is greater than zero, and that
  // the block is contained in the initial chunk.  Returns true if it succeeded
  // and false otherwise.
  bool CommitBlock(Address start, size_t size);

  // Checks if an allocated MemoryChunk was intended to be used for executable
  // memory.
  bool IsMemoryChunkExecutable(MemoryChunk* chunk) {
    return executable_memory_.find(chunk) != executable_memory_.end();
  }

  // Uncommit a contiguous block of memory [start..(start+size)[.
  // start is not kNullAddress, the size is greater than zero, and the
  // block is contained in the initial chunk.  Returns true if it succeeded
  // and false otherwise.
  bool UncommitBlock(Address start, size_t size);

  // Zaps a contiguous block of memory [start..(start+size)[ with
  // a given zap value.
  void ZapBlock(Address start, size_t size, uintptr_t zap_value);

  V8_WARN_UNUSED_RESULT bool CommitExecutableMemory(VirtualMemory* vm,
                                                    Address start,
                                                    size_t commit_size,
                                                    size_t reserved_size);

  CodeRange* code_range() { return code_range_; }
  Unmapper* unmapper() { return &unmapper_; }

 private:
  // PreFree logically frees the object, i.e., it takes care of the size
  // bookkeeping and calls the allocation callback.
  void PreFreeMemory(MemoryChunk* chunk);

  // FreeMemory can be called concurrently when PreFree was executed before.
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
  CodeRange* code_range_;

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

  friend class heap::TestCodeRangeScope;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryAllocator);
};

extern template Page*
MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, PagedSpace>(
    size_t size, PagedSpace* owner, Executability executable);
extern template Page*
MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, SemiSpace>(
    size_t size, SemiSpace* owner, Executability executable);
extern template Page*
MemoryAllocator::AllocatePage<MemoryAllocator::kPooled, SemiSpace>(
    size_t size, SemiSpace* owner, Executability executable);

// -----------------------------------------------------------------------------
// Interface for heap object iterator to be implemented by all object space
// object iterators.
//
// NOTE: The space specific object iterators also implements the own next()
//       method which is used to avoid using virtual functions
//       iterating a specific space.

class V8_EXPORT_PRIVATE ObjectIterator : public Malloced {
 public:
  virtual ~ObjectIterator() {}
  virtual HeapObject* Next() = 0;
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

typedef PageIteratorImpl<Page> PageIterator;
typedef PageIteratorImpl<LargePage> LargePageIterator;

class PageRange {
 public:
  typedef PageIterator iterator;
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
// A HeapObjectIterator iterates objects from the bottom of the given space
// to its top or from the bottom of the given page to its top.
//
// If objects are allocated in the page during iteration the iterator may
// or may not iterate over those objects.  The caller must create a new
// iterator in order to be sure to visit these new objects.
class V8_EXPORT_PRIVATE HeapObjectIterator : public ObjectIterator {
 public:
  // Creates a new object iterator in a given space.
  explicit HeapObjectIterator(PagedSpace* space);
  explicit HeapObjectIterator(Page* page);

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns nullptr when the iteration has ended.
  inline HeapObject* Next() override;

 private:
  // Fast (inlined) path of next().
  inline HeapObject* FromCurrentPage();

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
class AllocationStats BASE_EMBEDDED {
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

// A free list maintaining free blocks of memory. The free list is organized in
// a way to encourage objects allocated around the same time to be near each
// other. The normal way to allocate is intended to be by bumping a 'top'
// pointer until it hits a 'limit' pointer.  When the limit is hit we need to
// find a new space to allocate from. This is done with the free list, which is
// divided up into rough categories to cut down on waste. Having finer
// categories would scatter allocation more.

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
class V8_EXPORT_PRIVATE FreeList {
 public:
  // This method returns how much memory can be allocated after freeing
  // maximum_freed memory.
  static inline size_t GuaranteedAllocatable(size_t maximum_freed) {
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

  static FreeListCategoryType SelectFreeListCategoryType(size_t size_in_bytes) {
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

  FreeList();

  // Adds a node on the free list. The block of size {size_in_bytes} starting
  // at {start} is placed on the free list. The return value is the number of
  // bytes that were not added to the free list, because they freed memory block
  // was too small. Bookkeeping information will be written to the block, i.e.,
  // its contents will be destroyed. The start address should be word aligned,
  // and the size should be a non-zero multiple of the word size.
  size_t Free(Address start, size_t size_in_bytes, FreeMode mode);

  // Allocates a free space node frome the free list of at least size_in_bytes
  // bytes. Returns the actual node size in node_size which can be bigger than
  // size_in_bytes. This method returns null if the allocation request cannot be
  // handled by the free list.
  V8_WARN_UNUSED_RESULT FreeSpace* Allocate(size_t size_in_bytes,
                                            size_t* node_size);

  // Clear the free list.
  void Reset();

  void ResetStats() {
    wasted_bytes_ = 0;
    ForAllFreeListCategories(
        [](FreeListCategory* category) { category->ResetStats(); });
  }

  // Return the number of bytes available on the free list.
  size_t Available() {
    size_t available = 0;
    ForAllFreeListCategories([&available](FreeListCategory* category) {
      available += category->available();
    });
    return available;
  }

  bool IsEmpty() {
    bool empty = true;
    ForAllFreeListCategories([&empty](FreeListCategory* category) {
      if (!category->is_empty()) empty = false;
    });
    return empty;
  }

  // Used after booting the VM.
  void RepairLists(Heap* heap);

  size_t EvictFreeListItems(Page* page);
  bool ContainsPageFreeListItems(Page* page);

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
    for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
      ForAllFreeListCategories(static_cast<FreeListCategoryType>(i), callback);
    }
  }

  bool AddCategory(FreeListCategory* category);
  void RemoveCategory(FreeListCategory* category);
  void PrintCategories(FreeListCategoryType type);

  // Returns a page containing an entry for a given type, or nullptr otherwise.
  inline Page* GetPageForCategoryType(FreeListCategoryType type);

#ifdef DEBUG
  size_t SumFreeLists();
  bool IsVeryLong();
#endif

 private:
  class FreeListCategoryIterator {
   public:
    FreeListCategoryIterator(FreeList* free_list, FreeListCategoryType type)
        : current_(free_list->categories_[type]) {}

    bool HasNext() { return current_ != nullptr; }

    FreeListCategory* Next() {
      DCHECK(HasNext());
      FreeListCategory* tmp = current_;
      current_ = current_->next();
      return tmp;
    }

   private:
    FreeListCategory* current_;
  };

  // The size range of blocks, in bytes.
  static const size_t kMinBlockSize = 3 * kPointerSize;
  static const size_t kMaxBlockSize = Page::kAllocatableMemory;

  static const size_t kTiniestListMax = 0xa * kPointerSize;
  static const size_t kTinyListMax = 0x1f * kPointerSize;
  static const size_t kSmallListMax = 0xff * kPointerSize;
  static const size_t kMediumListMax = 0x7ff * kPointerSize;
  static const size_t kLargeListMax = 0x3fff * kPointerSize;
  static const size_t kTinyAllocationMax = kTiniestListMax;
  static const size_t kSmallAllocationMax = kTinyListMax;
  static const size_t kMediumAllocationMax = kSmallListMax;
  static const size_t kLargeAllocationMax = kMediumListMax;

  // Walks all available categories for a given |type| and tries to retrieve
  // a node. Returns nullptr if the category is empty.
  FreeSpace* FindNodeIn(FreeListCategoryType type, size_t minimum_size,
                        size_t* node_size);

  // Tries to retrieve a node from the first category in a given |type|.
  // Returns nullptr if the category is empty or the top entry is smaller
  // than minimum_size.
  FreeSpace* TryFindNodeIn(FreeListCategoryType type, size_t minimum_size,
                           size_t* node_size);

  // Searches a given |type| for a node of at least |minimum_size|.
  FreeSpace* SearchForNodeInList(FreeListCategoryType type, size_t* node_size,
                                 size_t minimum_size);

  // The tiny categories are not used for fast allocation.
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

  FreeListCategory* top(FreeListCategoryType type) const {
    return categories_[type];
  }

  std::atomic<size_t> wasted_bytes_;
  FreeListCategory* categories_[kNumberOfCategories];

  friend class FreeListCategory;
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
  static inline LocalAllocationBuffer InvalidBuffer();

  // Creates a new LAB from a given {AllocationResult}. Results in
  // InvalidBuffer if the result indicates a retry.
  static inline LocalAllocationBuffer FromResult(Heap* heap,
                                                 AllocationResult result,
                                                 intptr_t size);

  ~LocalAllocationBuffer() { Close(); }

  // Convert to C++11 move-semantics once allowed by the style guide.
  LocalAllocationBuffer(const LocalAllocationBuffer& other);
  LocalAllocationBuffer& operator=(const LocalAllocationBuffer& other);

  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment);

  inline bool IsValid() { return allocation_info_.top() != kNullAddress; }

  // Try to merge LABs, which is only possible when they are adjacent in memory.
  // Returns true if the merge was successful, false otherwise.
  inline bool TryMerge(LocalAllocationBuffer* other);

  inline bool TryFreeLast(HeapObject* object, int object_size);

  // Close a LAB, effectively invalidating it. Returns the unused area.
  LinearAllocationArea Close();

 private:
  LocalAllocationBuffer(Heap* heap, LinearAllocationArea allocation_info);

  Heap* heap_;
  LinearAllocationArea allocation_info_;
};

class SpaceWithLinearArea : public Space {
 public:
  SpaceWithLinearArea(Heap* heap, AllocationSpace id)
      : Space(heap, id), top_on_previous_step_(0) {
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
};

class V8_EXPORT_PRIVATE PagedSpace
    : NON_EXPORTED_BASE(public SpaceWithLinearArea) {
 public:
  typedef PageIterator iterator;

  static const size_t kCompactionMemoryWanted = 500 * KB;

  // Creates a space with an id.
  PagedSpace(Heap* heap, AllocationSpace id, Executability executable);

  ~PagedSpace() override { TearDown(); }

  // Checks whether an object/address is in this space.
  inline bool Contains(Address a);
  inline bool Contains(Object* o);
  bool ContainsSlow(Address addr);

  // Does the space need executable memory?
  Executability executable() { return executable_; }

  // During boot the free_space_map is created, and afterwards we may need
  // to write it into the free list nodes that were already created.
  void RepairFreeListsAfterDeserialization();

  // Prepares for a mark-compact GC.
  void PrepareForMarkCompact();

  // Current capacity without growing (Size() + Available()).
  size_t Capacity() { return accounting_stats_.Capacity(); }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  void ResetFreeListStatistics();

  // Sets the capacity, the available space and the wasted space to zero.
  // The stats are rebuilt during sweeping by adding each page to the
  // capacity and the size when it is encountered.  As free spaces are
  // discovered during the sweeping they are subtracted from the size and added
  // to the available and wasted totals.
  void ClearStats() {
    accounting_stats_.ClearSize();
    free_list_.ResetStats();
    ResetFreeListStatistics();
  }

  // Available bytes without growing.  These are the bytes on the free list.
  // The bytes in the linear allocation area are not included in this total
  // because updating the stats would slow down allocation.  New pages are
  // immediately added to the free list so they show up here.
  size_t Available() override { return free_list_.Available(); }

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
  virtual size_t Waste() { return free_list_.wasted_bytes(); }

  enum UpdateSkipList { UPDATE_SKIP_LIST, IGNORE_SKIP_LIST };

  // Allocate the requested number of bytes in the space if possible, return a
  // failure object if not. Only use IGNORE_SKIP_LIST if the skip list is going
  // to be manually updated later.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawUnaligned(
      int size_in_bytes, UpdateSkipList update_skip_list = UPDATE_SKIP_LIST);

  // Allocate the requested number of bytes in the space double aligned if
  // possible, return a failure object if not.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment);

  // Allocate the requested number of bytes in the space and consider allocation
  // alignment if needed.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationAlignment alignment);

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
    size_t wasted = free_list_.Free(start, size_in_bytes, kLinkCategory);
    Page* page = Page::FromAddress(start);
    accounting_stats_.DecreaseAllocatedBytes(size_in_bytes, page);
    DCHECK_GE(size_in_bytes, wasted);
    return size_in_bytes - wasted;
  }

  size_t UnaccountedFree(Address start, size_t size_in_bytes) {
    size_t wasted = free_list_.Free(start, size_in_bytes, kDoNotLinkCategory);
    DCHECK_GE(size_in_bytes, wasted);
    return size_in_bytes - wasted;
  }

  inline bool TryFreeLast(HeapObject* object, int object_size);

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

  Page* InitializePage(MemoryChunk* chunk, Executability executable);

  void ReleasePage(Page* page);

  // Adds the page to this space and returns the number of bytes added to the
  // free list of the space.
  size_t AddPage(Page* page);
  void RemovePage(Page* page);
  // Remove a page if it has at least |size_in_bytes| bytes available that can
  // be used for allocation.
  Page* RemovePageSafe(int size_in_bytes);

  void SetReadAndExecutable();
  void SetReadAndWritable();

#ifdef VERIFY_HEAP
  // Verify integrity of this space.
  virtual void Verify(Isolate* isolate, ObjectVisitor* visitor);

  void VerifyLiveBytes();

  // Overridden by subclasses to verify space-specific object
  // properties (e.g., only maps or free-list nodes are in map space).
  virtual void VerifyObject(HeapObject* obj) {}
#endif

#ifdef DEBUG
  void VerifyCountersAfterSweeping();
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

  virtual bool is_local() { return false; }

  // Merges {other} into the current space. Note that this modifies {other},
  // e.g., removes its bump pointer area and resets statistics.
  void MergeCompactionSpace(CompactionSpace* other);

  // Refills the free list from the corresponding free list filled by the
  // sweeper.
  virtual void RefillFreeList();

  FreeList* free_list() { return &free_list_; }

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

  std::unique_ptr<ObjectIterator> GetObjectIterator() override;

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
    return identity() == OLD_SPACE && !is_local();
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
  inline bool EnsureLinearAllocationArea(int size_in_bytes);
  // Allocates an object from the linear allocation area. Assumes that the
  // linear allocation area is large enought to fit the object.
  inline HeapObject* AllocateLinearly(int size_in_bytes);
  // Tries to allocate an aligned object from the linear allocation area.
  // Returns nullptr if the linear allocation area does not fit the object.
  // Otherwise, returns the object pointer and writes the allocation size
  // (object size + alignment filler size) to the size_in_bytes.
  inline HeapObject* TryAllocateLinearlyAligned(int* size_in_bytes,
                                                AllocationAlignment alignment);

  V8_WARN_UNUSED_RESULT bool RefillLinearAllocationAreaFromFreeList(
      size_t size_in_bytes);

  // If sweeping is still in progress try to sweep unswept pages. If that is
  // not successful, wait for the sweeper threads and retry free-list
  // allocation. Returns false if there is not enough space and the caller
  // has to retry after collecting garbage.
  V8_WARN_UNUSED_RESULT virtual bool SweepAndRetryAllocation(int size_in_bytes);

  // Slow path of AllocateRaw. This function is space-dependent. Returns false
  // if there is not enough space and the caller has to retry after
  // collecting garbage.
  V8_WARN_UNUSED_RESULT virtual bool SlowRefillLinearAllocationArea(
      int size_in_bytes);

  // Implementation of SlowAllocateRaw. Returns false if there is not enough
  // space and the caller has to retry after collecting garbage.
  V8_WARN_UNUSED_RESULT bool RawSlowRefillLinearAllocationArea(
      int size_in_bytes);

  Executability executable_;

  size_t area_size_;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  // The space's free list.
  FreeList free_list_;

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
  typedef PageIterator iterator;

  static void Swap(SemiSpace* from, SemiSpace* to);

  SemiSpace(Heap* heap, SemiSpaceId semispace)
      : Space(heap, NEW_SPACE),
        current_capacity_(0),
        maximum_capacity_(0),
        minimum_capacity_(0),
        age_mark_(kNullAddress),
        committed_(false),
        id_(semispace),
        current_page_(nullptr),
        pages_used_(0) {}

  inline bool Contains(HeapObject* o);
  inline bool Contains(Object* o);
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

  Page* InitializePage(MemoryChunk* chunk, Executability executable);

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

  size_t Size() override {
    UNREACHABLE();
  }

  size_t SizeOfObjects() override { return Size(); }

  size_t Available() override {
    UNREACHABLE();
  }

  Page* first_page() { return reinterpret_cast<Page*>(Space::first_page()); }
  Page* last_page() { return reinterpret_cast<Page*>(Space::last_page()); }

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  std::unique_ptr<ObjectIterator> GetObjectIterator() override;

#ifdef DEBUG
  void Print() override;
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
  friend class SemiSpaceIterator;
};


// A SemiSpaceIterator is an ObjectIterator that iterates over the active
// semispace of the heap's new space.  It iterates over the objects in the
// semispace from a given start address (defaulting to the bottom of the
// semispace) to the top of the semispace.  New objects allocated after the
// iterator is created are not iterated.
class SemiSpaceIterator : public ObjectIterator {
 public:
  // Create an iterator over the allocated objects in the given to-space.
  explicit SemiSpaceIterator(NewSpace* space);

  inline HeapObject* Next() override;

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

class NewSpace : public SpaceWithLinearArea {
 public:
  typedef PageIterator iterator;

  NewSpace(Heap* heap, size_t initial_semispace_capacity,
           size_t max_semispace_capacity);

  ~NewSpace() override { TearDown(); }

  inline bool Contains(HeapObject* o);
  inline bool ContainsSlow(Address a);
  inline bool Contains(Object* o);

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
  size_t Size() override {
    DCHECK_GE(top(), to_space_.page_low());
    return to_space_.pages_used() * Page::kAllocatableMemory +
           static_cast<size_t>(top() - to_space_.page_low());
  }

  size_t SizeOfObjects() override { return Size(); }

  // Return the allocatable capacity of a semispace.
  size_t Capacity() {
    SLOW_DCHECK(to_space_.current_capacity() == from_space_.current_capacity());
    return (to_space_.current_capacity() / Page::kPageSize) *
           Page::kAllocatableMemory;
  }

  // Return the current size of a semispace, allocatable and non-allocatable
  // memory.
  size_t TotalCapacity() {
    DCHECK(to_space_.current_capacity() == from_space_.current_capacity());
    return to_space_.current_capacity();
  }

  // Committed memory for NewSpace is the committed memory of both semi-spaces
  // combined.
  size_t CommittedMemory() override {
    return from_space_.CommittedMemory() + to_space_.CommittedMemory();
  }

  size_t MaximumCommittedMemory() override {
    return from_space_.MaximumCommittedMemory() +
           to_space_.MaximumCommittedMemory();
  }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  // Return the available bytes without growing.
  size_t Available() override {
    DCHECK_GE(Capacity(), Size());
    return Capacity() - Size();
  }

  size_t ExternalBackingStoreBytes(
      ExternalBackingStoreType type) const override {
    DCHECK_EQ(0, from_space_.ExternalBackingStoreBytes(type));
    return to_space_.ExternalBackingStoreBytes(type);
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
      allocated += Page::kAllocatableMemory;
      current_page = current_page->next_page();
    }
    DCHECK_GE(top(), current_page->area_start());
    allocated += top() - current_page->area_start();
    DCHECK_LE(allocated, Size());
    return allocated;
  }

  void MovePageFromSpaceToSpace(Page* page) {
    DCHECK(page->InFromSpace());
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
    DCHECK_GE(top(), original_top());
    DCHECK_LE(top(), original_limit());
    original_top_ = top();
  }

  Address original_top() { return original_top_; }
  Address original_limit() { return original_limit_; }

  // Return the address of the first allocatable address in the active
  // semispace. This may be the address where the first object resides.
  Address first_allocatable_address() { return to_space_.space_start(); }

  // Get the age mark of the inactive semispace.
  Address age_mark() { return from_space_.age_mark(); }
  // Set the age mark in the active semispace.
  void set_age_mark(Address mark) { to_space_.set_age_mark(mark); }

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRawAligned(int size_in_bytes, AllocationAlignment alignment);

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRawUnaligned(int size_in_bytes);

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationAlignment alignment);

  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawSynchronized(
      int size_in_bytes, AllocationAlignment alignment);

  // Reset the allocation pointer to the beginning of the active semispace.
  void ResetLinearAllocationArea();

  // When inline allocation stepping is active, either because of incremental
  // marking, idle scavenge, or allocation statistics gathering, we 'interrupt'
  // inline allocation every once in a while. This is done by setting
  // allocation_info_.limit to be lower than the actual limit and and increasing
  // it in steps to guarantee that the observers are notified periodically.
  void UpdateInlineAllocationLimit(size_t size_in_bytes) override;

  inline bool ToSpaceContainsSlow(Address a);
  inline bool FromSpaceContainsSlow(Address a);
  inline bool ToSpaceContains(Object* o);
  inline bool FromSpaceContains(Object* o);

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

  std::unique_ptr<ObjectIterator> GetObjectIterator() override;

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

  friend class SemiSpaceIterator;
};

class PauseAllocationObserversScope {
 public:
  explicit PauseAllocationObserversScope(Heap* heap);
  ~PauseAllocationObserversScope();

 private:
  Heap* heap_;
  DISALLOW_COPY_AND_ASSIGN(PauseAllocationObserversScope);
};

// -----------------------------------------------------------------------------
// Compaction space that is used temporarily during compaction.

class V8_EXPORT_PRIVATE CompactionSpace : public PagedSpace {
 public:
  CompactionSpace(Heap* heap, AllocationSpace id, Executability executable)
      : PagedSpace(heap, id, executable) {}

  bool is_local() override { return true; }

 protected:
  // The space is temporary and not included in any snapshots.
  bool snapshotable() override { return false; }

  V8_WARN_UNUSED_RESULT bool SweepAndRetryAllocation(
      int size_in_bytes) override;

  V8_WARN_UNUSED_RESULT bool SlowRefillLinearAllocationArea(
      int size_in_bytes) override;
};


// A collection of |CompactionSpace|s used by a single compaction task.
class CompactionSpaceCollection : public Malloced {
 public:
  explicit CompactionSpaceCollection(Heap* heap)
      : old_space_(heap, OLD_SPACE, Executability::NOT_EXECUTABLE),
        code_space_(heap, CODE_SPACE, Executability::EXECUTABLE) {}

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
  explicit OldSpace(Heap* heap) : PagedSpace(heap, OLD_SPACE, NOT_EXECUTABLE) {}
};

// -----------------------------------------------------------------------------
// Old generation code object space.

class CodeSpace : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  explicit CodeSpace(Heap* heap) : PagedSpace(heap, CODE_SPACE, EXECUTABLE) {}
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
  explicit MapSpace(Heap* heap) : PagedSpace(heap, MAP_SPACE, NOT_EXECUTABLE) {}

  int RoundSizeDownToObjectAlignment(int size) override {
    if (base::bits::IsPowerOfTwo(Map::kSize)) {
      return RoundDown(size, Map::kSize);
    } else {
      return (size / Map::kSize) * Map::kSize;
    }
  }

#ifdef VERIFY_HEAP
  void VerifyObject(HeapObject* obj) override;
#endif
};

// -----------------------------------------------------------------------------
// Read Only space for all Immortal Immovable and Immutable objects

class ReadOnlySpace : public PagedSpace {
 public:
  class WritableScope {
   public:
    explicit WritableScope(ReadOnlySpace* space) : space_(space) {
      space_->MarkAsReadWrite();
    }

    ~WritableScope() { space_->MarkAsReadOnly(); }

   private:
    ReadOnlySpace* space_;
  };

  explicit ReadOnlySpace(Heap* heap);

  bool writable() const { return !is_marked_read_only_; }

  void ClearStringPaddingIfNeeded();
  void MarkAsReadOnly();

 private:
  void MarkAsReadWrite();
  void SetPermissionsForPages(PageAllocator::Permission access);

  bool is_marked_read_only_ = false;
  //
  // String padding must be cleared just before serialization and therefore the
  // string padding in the space will already have been cleared if the space was
  // deserialized.
  bool is_string_padding_cleared_;
};

// -----------------------------------------------------------------------------
// Large objects ( > kMaxRegularHeapObjectSize ) are allocated and
// managed by the large object space. A large object is allocated from OS
// heap with extra padding bytes (Page::kPageSize + Page::kObjectStartOffset).
// A large object always starts at Page::kObjectStartOffset to a page.
// Large objects do not move during garbage collections.

class LargeObjectSpace : public Space {
 public:
  typedef LargePageIterator iterator;

  explicit LargeObjectSpace(Heap* heap);
  LargeObjectSpace(Heap* heap, AllocationSpace id);

  ~LargeObjectSpace() override { TearDown(); }

  // Releases internal resources, frees objects in this space.
  void TearDown();

  static size_t ObjectSizeFor(size_t chunk_size) {
    if (chunk_size <= (Page::kPageSize + Page::kObjectStartOffset)) return 0;
    return chunk_size - Page::kPageSize - Page::kObjectStartOffset;
  }

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRaw(int object_size,
                                                     Executability executable);

  // Available bytes for objects in this space.
  size_t Available() override;

  size_t Size() override { return size_; }
  size_t SizeOfObjects() override { return objects_size_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  int PageCount() { return page_count_; }

  // Finds an object for a given address, returns a Smi if it is not found.
  // The function iterates through all objects in this space, may be slow.
  Object* FindObject(Address a);

  // Takes the chunk_map_mutex_ and calls FindPage after that.
  LargePage* FindPageThreadSafe(Address a);

  // Finds a large object page containing the given address, returns nullptr
  // if such a page doesn't exist.
  LargePage* FindPage(Address a);

  // Clears the marking state of live objects.
  void ClearMarkingStateOfLiveObjects();

  // Frees unmarked objects.
  void FreeUnmarkedObjects();

  void InsertChunkMapEntries(LargePage* page);
  void RemoveChunkMapEntries(LargePage* page);
  void RemoveChunkMapEntries(LargePage* page, Address free_start);

  // Checks whether a heap object is in this space; O(1).
  bool Contains(HeapObject* obj);
  // Checks whether an address is in the object area in this space. Iterates
  // all objects in the space. May be slow.
  bool ContainsSlow(Address addr) { return FindObject(addr)->IsHeapObject(); }

  // Checks whether the space is empty.
  bool IsEmpty() { return first_page() == nullptr; }

  LargePage* first_page() {
    return reinterpret_cast<LargePage*>(Space::first_page());
  }

  // Collect code statistics.
  void CollectCodeStatistics();

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  std::unique_ptr<ObjectIterator> GetObjectIterator() override;

  base::Mutex* chunk_map_mutex() { return &chunk_map_mutex_; }

#ifdef VERIFY_HEAP
  virtual void Verify(Isolate* isolate);
#endif

#ifdef DEBUG
  void Print() override;
#endif

 protected:
  LargePage* AllocateLargePage(int object_size, Executability executable);

 private:
  size_t size_;          // allocated bytes
  int page_count_;       // number of chunks
  size_t objects_size_;  // size of objects

  // The chunk_map_mutex_ has to be used when the chunk map is accessed
  // concurrently.
  base::Mutex chunk_map_mutex_;

  // Page-aligned addresses to their corresponding LargePage.
  std::unordered_map<Address, LargePage*> chunk_map_;

  friend class LargeObjectIterator;
};

class NewLargeObjectSpace : public LargeObjectSpace {
 public:
  explicit NewLargeObjectSpace(Heap* heap);

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRaw(int object_size);

  // Available bytes for objects in this space.
  size_t Available() override;
};

class LargeObjectIterator : public ObjectIterator {
 public:
  explicit LargeObjectIterator(LargeObjectSpace* space);

  HeapObject* Next() override;

 private:
  LargePage* current_;
};

// Iterates over the chunks (pages and large object pages) that can contain
// pointers to new space or to evacuation candidates.
class MemoryChunkIterator BASE_EMBEDDED {
 public:
  inline explicit MemoryChunkIterator(Heap* heap);

  // Return nullptr when the iterator is done.
  inline MemoryChunk* next();

 private:
  enum State {
    kOldSpaceState,
    kMapState,
    kCodeState,
    kLargeObjectState,
    kFinishedState
  };
  Heap* heap_;
  State state_;
  PageIterator old_iterator_;
  PageIterator code_iterator_;
  PageIterator map_iterator_;
  LargePageIterator lo_iterator_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_H_
