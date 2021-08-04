// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_H_
#define V8_HEAP_MEMORY_CHUNK_H_

#include <atomic>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap.h"
#include "src/heap/invalidated-slots.h"
#include "src/heap/list.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/slot-set.h"

namespace v8 {
namespace internal {

class CodeObjectRegistry;
class FreeListCategory;

// MemoryChunk represents a memory region owned by a specific space.
// It is divided into the header and the body. Chunk start is always
// 1MB aligned. Start of the body is aligned so it can accommodate
// any heap object.
class MemoryChunk : public BasicMemoryChunk {
 public:
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

  static const size_t kHeaderSize = MemoryChunkLayout::kMemoryChunkHeaderSize;

  static const intptr_t kOldToNewSlotSetOffset =
      MemoryChunkLayout::kSlotSetOffset;

  // Page size in bytes.  This must be a multiple of the OS page size.
  static const int kPageSize = 1 << kPageSizeBits;

  // Maximum number of nested code memory modification scopes.
  static const int kMaxWriteUnprotectCounter = 3;

  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromAddress(Address a) {
    return cast(BasicMemoryChunk::FromAddress(a));
  }

  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromHeapObject(HeapObject o) {
    return cast(BasicMemoryChunk::FromHeapObject(o));
  }

  static MemoryChunk* cast(BasicMemoryChunk* chunk) {
    SLOW_DCHECK(!chunk->InReadOnlySpace());
    return static_cast<MemoryChunk*>(chunk);
  }

  static const MemoryChunk* cast(const BasicMemoryChunk* chunk) {
    SLOW_DCHECK(!chunk->InReadOnlySpace());
    return static_cast<const MemoryChunk*>(chunk);
  }

  size_t buckets() const { return SlotSet::BucketsForSize(size()); }

  void SetOldGenerationPageFlags(bool is_marking);
  void SetYoungGenerationPageFlags(bool is_marking);

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

  void AllocateYoungGenerationBitmap();
  void ReleaseYoungGenerationBitmap();

  int FreeListsLength();

  // Approximate amount of physical memory committed for this chunk.
  V8_EXPORT_PRIVATE size_t CommittedPhysicalMemory();

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

  Space* owner() const {
    return reinterpret_cast<Space*>(BasicMemoryChunk::owner());
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
  const heap::ListNode<MemoryChunk>& list_node() const { return list_node_; }

  CodeObjectRegistry* GetCodeObjectRegistry() { return code_object_registry_; }

  PossiblyEmptyBuckets* possibly_empty_buckets() {
    return &possibly_empty_buckets_;
  }

  // Release memory allocated by the chunk, except that which is needed by
  // read-only space chunks.
  void ReleaseAllocatedMemoryNeededForWritableChunk();

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  ObjectStartBitmap* object_start_bitmap() { return &object_start_bitmap_; }
#endif

 protected:
  static MemoryChunk* Initialize(BasicMemoryChunk* basic_chunk, Heap* heap,
                                 Executability executable);

  // Release all memory allocated by the chunk. Should be called when memory
  // chunk is about to be freed.
  void ReleaseAllAllocatedMemory();

  // Sets the requested page permissions only if the write unprotect counter
  // has reached 0.
  void DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::Permission permission);

  template <AccessMode mode>
  ConcurrentBitmap<mode>* young_generation_bitmap() const {
    return reinterpret_cast<ConcurrentBitmap<mode>*>(young_generation_bitmap_);
  }
#ifdef DEBUG
  static void ValidateOffsets(MemoryChunk* chunk);
#endif

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES];

  // Used by the incremental marker to keep track of the scanning progress in
  // large objects that have a progress bar and are scanned in increments.
  std::atomic<size_t> progress_bar_;

  // Count of bytes marked black on page.
  std::atomic<intptr_t> live_byte_count_;

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* sweeping_slot_set_;
  TypedSlotSet* typed_slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES];
  InvalidatedSlots* invalidated_slots_[NUMBER_OF_REMEMBERED_SET_TYPES];

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

  // Tracks off-heap memory used by this memory chunk.
  std::atomic<size_t> external_backing_store_bytes_[kNumTypes];

  heap::ListNode<MemoryChunk> list_node_;

  FreeListCategory** categories_;

  std::atomic<intptr_t> young_generation_live_byte_count_;
  Bitmap* young_generation_bitmap_;

  CodeObjectRegistry* code_object_registry_;

  PossiblyEmptyBuckets possibly_empty_buckets_;

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  ObjectStartBitmap object_start_bitmap_;
#endif

 private:
  friend class ConcurrentMarkingState;
  friend class MajorMarkingState;
  friend class MajorAtomicMarkingState;
  friend class MajorNonAtomicMarkingState;
  friend class MemoryAllocator;
  friend class MemoryChunkValidator;
  friend class MinorMarkingState;
  friend class MinorNonAtomicMarkingState;
  friend class PagedSpace;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_CHUNK_H_
