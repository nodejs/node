// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASIC_MEMORY_CHUNK_H_
#define V8_HEAP_BASIC_MEMORY_CHUNK_H_

#include <type_traits>
#include <unordered_map>

#include "src/base/atomic-utils.h"
#include "src/base/flags.h"
#include "src/base/functional.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/objects/heap-object.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class BaseSpace;

class BasicMemoryChunk {
 public:
  // All possible flags that can be set on a page. While the value of flags
  // doesn't matter in principle, keep flags used in the write barrier together
  // in order to have dense page flag checks in the write barrier.
  enum Flag : uintptr_t {
    NO_FLAGS = 0u,

    // This page belongs to a shared heap.
    IN_WRITABLE_SHARED_SPACE = 1u << 0,

    // These two flags are used in the write barrier to catch "interesting"
    // references.
    POINTERS_TO_HERE_ARE_INTERESTING = 1u << 1,
    POINTERS_FROM_HERE_ARE_INTERESTING = 1u << 2,

    // A page in the from-space or a young large page that was not scavenged
    // yet.
    FROM_PAGE = 1u << 3,
    // A page in the to-space or a young large page that was scavenged.
    TO_PAGE = 1u << 4,

    // |INCREMENTAL_MARKING|: Indicates whether incremental marking is currently
    // enabled.
    INCREMENTAL_MARKING = 1u << 5,

    // The memory chunk belongs to the read-only heap and does not participate
    // in garbage collection. This is used instead of owner for identity
    // checking since read-only chunks have no owner once they are detached.
    READ_ONLY_HEAP = 1u << 6,

    // ----------------------------------------------------------------
    // Values below here are not critical for the heap write barrier.

    LARGE_PAGE = 1u << 7,
    EVACUATION_CANDIDATE = 1u << 8,
    NEVER_EVACUATE = 1u << 9,

    // |PAGE_NEW_OLD_PROMOTION|: A page tagged with this flag has been promoted
    // from new to old space during evacuation.
    PAGE_NEW_OLD_PROMOTION = 1u << 10,

    // This flag is intended to be used for testing. Works only when both
    // v8_flags.stress_compaction and
    // v8_flags.manual_evacuation_candidates_selection are set. It forces the
    // page to become an evacuation candidate at next candidates selection
    // cycle.
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

    NEW_SPACE_BELOW_AGE_MARK = 1u << 16,

    // The memory chunk freeing bookkeeping has been performed but the chunk has
    // not yet been freed.
    UNREGISTERED = 1u << 17,

    // The memory chunk is pinned in memory and can't be moved. This is likely
    // because there exists a potential pointer to somewhere in the chunk which
    // can't be updated.
    PINNED = 1u << 18,

    // A Page with code objects.
    IS_EXECUTABLE = 1u << 19,
  };

  using MainThreadFlags = base::Flags<Flag, uintptr_t>;

  static constexpr MainThreadFlags kAllFlagsMask = ~MainThreadFlags(NO_FLAGS);
  static constexpr MainThreadFlags kPointersToHereAreInterestingMask =
      POINTERS_TO_HERE_ARE_INTERESTING;
  static constexpr MainThreadFlags kPointersFromHereAreInterestingMask =
      POINTERS_FROM_HERE_ARE_INTERESTING;
  static constexpr MainThreadFlags kEvacuationCandidateMask =
      EVACUATION_CANDIDATE;
  static constexpr MainThreadFlags kIsInYoungGenerationMask =
      MainThreadFlags(FROM_PAGE) | MainThreadFlags(TO_PAGE);
  static constexpr MainThreadFlags kIsLargePageMask = LARGE_PAGE;
  static constexpr MainThreadFlags kInSharedHeap = IN_WRITABLE_SHARED_SPACE;
  static constexpr MainThreadFlags kIncrementalMarking = INCREMENTAL_MARKING;
  static constexpr MainThreadFlags kSkipEvacuationSlotsRecordingMask =
      MainThreadFlags(kEvacuationCandidateMask) |
      MainThreadFlags(kIsInYoungGenerationMask);

  static constexpr intptr_t kAlignment =
      (static_cast<uintptr_t>(1) << kPageSizeBits);
  static constexpr intptr_t kAlignmentMask = kAlignment - 1;

  static constexpr intptr_t kSizeOffset = MemoryChunkLayout::kSizeOffset;
  static constexpr intptr_t kFlagsOffset = MemoryChunkLayout::kFlagsOffset;
  static constexpr intptr_t kHeapOffset = MemoryChunkLayout::kHeapOffset;
  static constexpr intptr_t kAreaStartOffset =
      MemoryChunkLayout::kAreaStartOffset;
  static constexpr intptr_t kAreaEndOffset = MemoryChunkLayout::kAreaEndOffset;
  static constexpr intptr_t kMarkingBitmapOffset =
      MemoryChunkLayout::kMarkingBitmapOffset;
  static constexpr size_t kHeaderSize =
      MemoryChunkLayout::kBasicMemoryChunkHeaderSize;

  static constexpr Address BaseAddress(Address a) {
    return a & ~kAlignmentMask;
  }

  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static BasicMemoryChunk* FromAddress(Address a) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<BasicMemoryChunk*>(BaseAddress(a));
  }

  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static BasicMemoryChunk* FromHeapObject(Tagged<HeapObject> o) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<BasicMemoryChunk*>(BaseAddress(o.ptr()));
  }

  static inline void UpdateHighWaterMark(Address mark) {
    if (mark == kNullAddress) return;
    // Need to subtract one from the mark because when a chunk is full the
    // top points to the next address after the chunk, which effectively belongs
    // to another chunk. See the comment to Page::FromAllocationAreaAddress.
    BasicMemoryChunk* chunk = BasicMemoryChunk::FromAddress(mark - 1);
    intptr_t new_mark = static_cast<intptr_t>(mark - chunk->address());
    intptr_t old_mark = chunk->high_water_mark_.load(std::memory_order_relaxed);
    while ((new_mark > old_mark) &&
           !chunk->high_water_mark_.compare_exchange_weak(
               old_mark, new_mark, std::memory_order_acq_rel)) {
    }
  }

  BasicMemoryChunk(Heap* heap, BaseSpace* space, size_t chunk_size,
                   Address area_start, Address area_end,
                   VirtualMemory reservation);

  Address address() const { return reinterpret_cast<Address>(this); }

  // Returns the offset of a given address to this page.
  inline size_t Offset(Address a) const {
    return static_cast<size_t>(a - address());
  }

  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }

  Address area_start() const { return area_start_; }

  Address area_end() const { return area_end_; }
  void set_area_end(Address area_end) { area_end_ = area_end; }

  size_t area_size() const {
    return static_cast<size_t>(area_end() - area_start());
  }

  Heap* heap() const {
    DCHECK_NOT_NULL(heap_);
    return heap_;
  }

  // Gets the chunk's owner or null if the space has been detached.
  BaseSpace* owner() const { return owner_; }
  void set_owner(BaseSpace* space) { owner_ = space; }

  // Return all current flags.
  MainThreadFlags GetFlags() const { return main_thread_flags_; }
  void SetFlag(Flag flag) { main_thread_flags_ |= flag; }
  bool IsFlagSet(Flag flag) const { return main_thread_flags_ & flag; }
  void ClearFlag(Flag flag) {
    main_thread_flags_ = main_thread_flags_.without(flag);
  }
  // Set or clear multiple flags at a time. `mask` indicates which flags are
  // should be replaced with new `flags`.
  void ClearFlags(MainThreadFlags flags) { main_thread_flags_ &= ~flags; }
  void SetFlags(MainThreadFlags flags, MainThreadFlags mask = kAllFlagsMask) {
    main_thread_flags_ = (main_thread_flags_ & ~mask) | (flags & mask);
  }

  bool InReadOnlySpace() const {
#ifdef THREAD_SANITIZER
    // This is needed because TSAN does not process the memory fence
    // emitted after page initialization.
    SynchronizedHeapLoad();
#endif
    return IsFlagSet(READ_ONLY_HEAP);
  }

  bool NeverEvacuate() const { return IsFlagSet(NEVER_EVACUATE); }
  void MarkNeverEvacuate() { SetFlag(NEVER_EVACUATE); }

  bool CanAllocate() const {
    return !IsEvacuationCandidate() && !IsFlagSet(NEVER_ALLOCATE_ON_PAGE);
  }

  bool IsEvacuationCandidate() const {
    DCHECK(!(IsFlagSet(NEVER_EVACUATE) && IsFlagSet(EVACUATION_CANDIDATE)));
    return IsFlagSet(EVACUATION_CANDIDATE);
  }

  bool ShouldSkipEvacuationSlotRecording() const {
    MainThreadFlags flags = GetFlags();
    return ((flags & kSkipEvacuationSlotsRecordingMask) != 0) &&
           ((flags & COMPACTION_WAS_ABORTED) == 0);
  }

  Executability executable() const {
    return IsFlagSet(IS_EXECUTABLE) ? EXECUTABLE : NOT_EXECUTABLE;
  }

  bool IsMarking() const { return IsFlagSet(INCREMENTAL_MARKING); }
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

  bool InWritableSharedSpace() const {
    return IsFlagSet(IN_WRITABLE_SHARED_SPACE);
  }

  bool IsWritable() const {
    // If this is a read-only space chunk but heap_ is non-null, it has not yet
    // been sealed and can be written to.
    return !InReadOnlySpace() || heap_ != nullptr;
  }

  bool IsPinned() const { return IsFlagSet(PINNED); }

  bool Contains(Address addr) const {
    return addr >= area_start() && addr < area_end();
  }

  // Checks whether |addr| can be a limit of addresses in this page. It's a
  // limit if it's in the page, or if it's just after the last byte of the page.
  bool ContainsLimit(Address addr) const {
    return addr >= area_start() && addr <= area_end();
  }

  size_t wasted_memory() const { return wasted_memory_; }
  void add_wasted_memory(size_t waste) { wasted_memory_ += waste; }
  size_t allocated_bytes() const { return allocated_bytes_; }

  Address HighWaterMark() const { return address() + high_water_mark_; }

  VirtualMemory* reserved_memory() { return &reservation_; }

  void ResetAllocationStatistics() {
    allocated_bytes_ = area_size();
    wasted_memory_ = 0;
  }

  void IncreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    allocated_bytes_ += bytes;
  }

  void DecreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    DCHECK_GE(allocated_bytes(), bytes);
    allocated_bytes_ -= bytes;
  }

#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race in
  // mark-bit initialization. See MemoryChunk::Initialize for the corresponding
  // release store.
  void SynchronizedHeapLoad() const;
#endif

 protected:
  // Overall size of the chunk, including the header and guards.
  size_t size_;

  // Flags that are only mutable from the main thread when no concurrent
  // component (e.g. marker, sweeper, compilation, allocation) is running.
  MainThreadFlags main_thread_flags_{NO_FLAGS};

  // TODO(v8:7464): Find a way to remove this.
  // This goes against the spirit for the BasicMemoryChunk, but until C++14/17
  // is the default it needs to live here because MemoryChunk is not standard
  // layout under C++11.
  Heap* heap_;

  // Start and end of allocatable memory on this chunk.
  Address area_start_;
  Address area_end_;

  // Byte allocated on the page, which includes all objects on the page and the
  // linear allocation area.
  size_t allocated_bytes_;
  // Freed memory that was not added to the free list.
  size_t wasted_memory_ = 0;

  // Assuming the initial allocation on a page is sequential, count highest
  // number of bytes ever allocated on the page.
  std::atomic<intptr_t> high_water_mark_;

  // The space owning this memory chunk.
  std::atomic<BaseSpace*> owner_;

  // If the chunk needs to remember its memory reservation, it is stored here.
  VirtualMemory reservation_;

  friend class BasicMemoryChunkValidator;
  friend class ConcurrentMarkingState;
  friend class MarkingState;
  friend class AtomicMarkingState;
  friend class NonAtomicMarkingState;
  friend class MemoryAllocator;
  friend class PagedSpace;
};

DEFINE_OPERATORS_FOR_FLAGS(BasicMemoryChunk::MainThreadFlags)

static_assert(std::is_standard_layout<BasicMemoryChunk>::value);

}  // namespace internal

namespace base {

// Define special hash function for chunk pointers, to be used with std data
// structures, e.g.
// std::unordered_set<BasicMemoryChunk*, base::hash<BasicMemoryChunk*>
// This hash function discards the trailing zero bits (chunk alignment).
// Notice that, when pointer compression is enabled, it also discards the
// cage base.
template <>
struct hash<const i::BasicMemoryChunk*> {
  V8_INLINE size_t operator()(const i::BasicMemoryChunk* chunk) const {
    return static_cast<v8::internal::Tagged_t>(
               reinterpret_cast<uintptr_t>(chunk)) >>
           kPageSizeBits;
  }
};

template <>
struct hash<i::BasicMemoryChunk*> {
  V8_INLINE size_t operator()(i::BasicMemoryChunk* chunk) const {
    return hash<const i::BasicMemoryChunk*>()(chunk);
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_HEAP_BASIC_MEMORY_CHUNK_H_
