// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_H_
#define V8_HEAP_MEMORY_CHUNK_H_

#include <set>
#include <vector>

#include "src/base/macros.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap.h"
#include "src/heap/invalidated-slots.h"
#include "src/heap/list.h"

namespace v8 {
namespace internal {

class CodeObjectRegistry;
class FreeListCategory;
class LocalArrayBufferTracker;

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

  static int MaxRegularCodeObjectSize();
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
      + kSystemPointerSize * 2  // heap::ListNode
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
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<MemoryChunk*>(BaseAddress(a));
  }
  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromHeapObject(HeapObject o) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
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
    intptr_t old_mark = chunk->high_water_mark_.load(std::memory_order_relaxed);
    while ((new_mark > old_mark) &&
           !chunk->high_water_mark_.compare_exchange_weak(
               old_mark, new_mark, std::memory_order_acq_rel)) {
    }
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

  heap::ListNode<MemoryChunk>& list_node() { return list_node_; }

  CodeObjectRegistry* GetCodeObjectRegistry() { return code_object_registry_; }

  PossiblyEmptyBuckets* possibly_empty_buckets() {
    return &possibly_empty_buckets_;
  }

  // Release memory allocated by the chunk, except that which is needed by
  // read-only space chunks.
  void ReleaseAllocatedMemoryNeededForWritableChunk();

 protected:
  static MemoryChunk* Initialize(Heap* heap, Address base, size_t size,
                                 Address area_start, Address area_end,
                                 Executability executable, Space* owner,
                                 VirtualMemory reservation);

  // Release all memory allocated by the chunk. Should be called when memory
  // chunk is about to be freed.
  void ReleaseAllAllocatedMemory();

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
  // they will be used immediately for an allocation. They are initialized
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

  heap::ListNode<MemoryChunk> list_node_;

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

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_CHUNK_H_
