// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_H_
#define V8_HEAP_MEMORY_CHUNK_H_

#include <atomic>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/base/active-system-pages.h"
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

  MemoryChunk(Heap* heap, BaseSpace* space, size_t size, Address area_start,
              Address area_end, VirtualMemory reservation,
              Executability executable, PageSize page_size);

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

  bool SweepingDone() const {
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
  V8_EXPORT_PRIVATE void RegisterObjectWithInvalidatedSlots(HeapObject object,
                                                            int new_size);
  template <RememberedSetType type>
  V8_EXPORT_PRIVATE void UpdateInvalidatedObjectSize(HeapObject object,
                                                     int new_size);
  template <RememberedSetType type>
  bool RegisteredObjectWithInvalidatedSlots(HeapObject object);
  template <RememberedSetType type>
  InvalidatedSlots* invalidated_slots() {
    return invalidated_slots_[type];
  }

  int FreeListsLength();

  // Approximate amount of physical memory committed for this chunk.
  V8_EXPORT_PRIVATE size_t CommittedPhysicalMemory() const;

  class ProgressBar& ProgressBar() {
    return progress_bar_;
  }
  const class ProgressBar& ProgressBar() const { return progress_bar_; }

  inline void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  inline void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) const {
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

  static PageAllocator::Permission GetCodeModificationPermission() {
    DCHECK(!V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT);
    // On MacOS on ARM64 RWX permissions are allowed to be set only when
    // fast W^X is enabled (see V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT).
    return !V8_HAS_PTHREAD_JIT_WRITE_PROTECT && v8_flags.write_code_using_rwx
               ? PageAllocator::kReadWriteExecute
               : PageAllocator::kReadWrite;
  }

  V8_EXPORT_PRIVATE void SetReadable();
  V8_EXPORT_PRIVATE void SetReadAndExecutable();

  V8_EXPORT_PRIVATE void SetCodeModificationPermissions();
  V8_EXPORT_PRIVATE void SetDefaultCodePermissions();

  heap::ListNode<MemoryChunk>& list_node() { return list_node_; }
  const heap::ListNode<MemoryChunk>& list_node() const { return list_node_; }

  CodeObjectRegistry* GetCodeObjectRegistry() { return code_object_registry_; }

  PossiblyEmptyBuckets* possibly_empty_buckets() {
    return &possibly_empty_buckets_;
  }

  // Release memory allocated by the chunk, except that which is needed by
  // read-only space chunks.
  void ReleaseAllocatedMemoryNeededForWritableChunk();

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
  ObjectStartBitmap* object_start_bitmap() { return &object_start_bitmap_; }

  const ObjectStartBitmap* object_start_bitmap() const {
    return &object_start_bitmap_;
  }
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_OSB

 protected:
  // Release all memory allocated by the chunk. Should be called when memory
  // chunk is about to be freed.
  void ReleaseAllAllocatedMemory();

  // Sets the requested page permissions only if the write unprotect counter
  // has reached 0.
  void DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::Permission permission);

#ifdef DEBUG
  static void ValidateOffsets(MemoryChunk* chunk);
#endif

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES];

  // Used by the marker to keep track of the scanning progress in large objects
  // that have a progress bar and are scanned in increments.
  class ProgressBar progress_bar_;

  // Count of bytes marked black on page.
  std::atomic<intptr_t> live_byte_count_;

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
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

  CodeObjectRegistry* code_object_registry_;

  PossiblyEmptyBuckets possibly_empty_buckets_;

  ActiveSystemPages active_system_pages_;

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
  ObjectStartBitmap object_start_bitmap_;
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_OSB

 private:
  friend class ConcurrentMarkingState;
  friend class MarkingState;
  friend class AtomicMarkingState;
  friend class NonAtomicMarkingState;
  friend class MemoryAllocator;
  friend class MemoryChunkValidator;
  friend class PagedSpace;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_CHUNK_H_
