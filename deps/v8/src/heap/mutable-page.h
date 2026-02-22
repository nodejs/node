// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MUTABLE_PAGE_H_
#define V8_HEAP_MUTABLE_PAGE_H_

#include <atomic>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/base-page.h"
#include "src/heap/base/active-system-pages.h"
#include "src/heap/list.h"
#include "src/heap/marking-progress-tracker.h"
#include "src/heap/marking.h"
#include "src/heap/slot-set.h"
#include "src/sandbox/check.h"

namespace v8 {
namespace internal {

class FreeListCategory;
class SlotSet;
class Space;

using ActiveSystemPages = ::heap::base::ActiveSystemPages;

enum class MarkingMode { kNoMarking, kMinorMarking, kMajorMarking };

enum RememberedSetType {
  OLD_TO_NEW,
  OLD_TO_NEW_BACKGROUND,
  OLD_TO_OLD,
  OLD_TO_SHARED,
  TRUSTED_TO_CODE,
  TRUSTED_TO_TRUSTED,
  TRUSTED_TO_SHARED_TRUSTED,
  SURVIVOR_TO_EXTERNAL_POINTER,
  NUMBER_OF_REMEMBERED_SET_TYPES
};

// A mutable page that represents a memory region owned by a specific space.
class MutablePage : public BasePage {
 public:
  enum class ConcurrentSweepingState : intptr_t {
    // The page state when sweeping is complete or sweeping must not be
    // performed on that page. Sweeper threads that are done with their work
    // will set this value and not touch the page anymore.
    kDone,
    // This page is ready for parallel sweeping.
    kPendingSweeping,
    // This page is ready for parallel promoted page.
    kPendingIteration,
    // This page is currently swept by a sweeper thread.
    kInProgress,
  };

  static PageAllocator::Permission GetCodeModificationPermission() {
    return v8_flags.jitless ? PageAllocator::kReadWrite
                            : PageAllocator::kReadWriteExecute;
  }

  // Only correct if the pointer is in the first kPageSize of the MemoryChunk.
  // This is not necessarily the case for large objects.
  V8_INLINE static MutablePage* FromAddress(Address a);
  V8_INLINE static MutablePage* FromAddress(const Isolate* i, Address a);

  // Objects pointers always point within the first kPageSize, so these calls
  // are always correct.
  V8_INLINE static MutablePage* FromHeapObject(const Isolate* i,
                                               Tagged<HeapObject> o);

  static MemoryChunk::MainThreadFlags OldGenerationPageFlags(
      MarkingMode marking_mode, AllocationSpace space);
  static MemoryChunk::MainThreadFlags YoungGenerationPageFlags(
      MarkingMode marking_mode);

  void SetOldGenerationPageFlags(MarkingMode marking_mode);
  void SetYoungGenerationPageFlags(MarkingMode marking_mode);
  V8_INLINE void SetMajorGCInProgress();
  V8_INLINE void ResetMajorGCInProgress();
  V8_INLINE void ClearFlagsNonExecutable(MemoryChunk::MainThreadFlags flags);
  V8_INLINE void SetFlagsNonExecutable(
      MemoryChunk::MainThreadFlags flags,
      MemoryChunk::MainThreadFlags mask = MemoryChunk::kAllFlagsMask);
  V8_INLINE void ClearFlagNonExecutable(MemoryChunk::Flag flag);
  V8_INLINE void SetFlagNonExecutable(MemoryChunk::Flag flag);
  void SetFlagMaybeExecutable(MemoryChunk::Flag flag);
  void ClearFlagMaybeExecutable(MemoryChunk::Flag flag);
  // TODO(mlippautz): Replace those with non-executable or slow versions.
  V8_INLINE void SetFlagUnlocked(MemoryChunk::Flag flag);
  V8_INLINE void ClearFlagUnlocked(MemoryChunk::Flag flag);

  V8_EXPORT_PRIVATE void MarkNeverEvacuate();

  size_t BucketsInSlotSet() const { return SlotSet::BucketsForSize(size()); }

  base::Mutex& mutex() { return mutex_; }
  const base::Mutex& mutex() const { return mutex_; }
  base::Mutex& object_mutex() { return object_mutex_; }
  const base::Mutex& object_mutex() const { return object_mutex_; }

  void set_concurrent_sweeping_state(ConcurrentSweepingState state) {
    concurrent_sweeping_ = state;
  }

  ConcurrentSweepingState concurrent_sweeping_state() {
    return static_cast<ConcurrentSweepingState>(concurrent_sweeping_.load());
  }

  bool SweepingDone() const {
    return concurrent_sweeping_ == ConcurrentSweepingState::kDone;
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  SlotSet* slot_set() {
    if constexpr (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&slot_set_[type]);
    return slot_set_[type];
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  const SlotSet* slot_set() const {
    return const_cast<MutablePage*>(this)->slot_set<type, access_mode>();
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  TypedSlotSet* typed_slot_set() {
    if constexpr (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(&typed_slot_set_[type]);
    return typed_slot_set_[type];
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  const TypedSlotSet* typed_slot_set() const {
    return const_cast<MutablePage*>(this)->typed_slot_set<type, access_mode>();
  }

  template <RememberedSetType type>
  bool ContainsSlots() const {
    return slot_set<type>() != nullptr || typed_slot_set<type>() != nullptr;
  }
  bool ContainsAnySlots() const;

  V8_EXPORT_PRIVATE SlotSet* AllocateSlotSet(RememberedSetType type);
  // Not safe to be called concurrently.
  void ReleaseSlotSet(RememberedSetType type);
  TypedSlotSet* AllocateTypedSlotSet(RememberedSetType type);
  // Not safe to be called concurrently.
  void ReleaseTypedSlotSet(RememberedSetType type);

  template <RememberedSetType type>
  SlotSet* ExtractSlotSet() {
    SlotSet* slot_set = slot_set_[type];
    // Conditionally reset to nullptr (instead of e.g. using std::exchange) to
    // avoid data races when transitioning from nullptr to nullptr.
    if (slot_set) {
      slot_set_[type] = nullptr;
    }
    return slot_set;
  }

  template <RememberedSetType type>
  TypedSlotSet* ExtractTypedSlotSet() {
    TypedSlotSet* typed_slot_set = typed_slot_set_[type];
    // Conditionally reset to nullptr (instead of e.g. using std::exchange) to
    // avoid data races when transitioning from nullptr to nullptr.
    if (typed_slot_set) {
      typed_slot_set_[type] = nullptr;
    }
    return typed_slot_set;
  }

  int ComputeFreeListsLength();

  // Approximate amount of physical memory committed for this chunk.
  V8_EXPORT_PRIVATE size_t CommittedPhysicalMemory() const;

  MarkingProgressTracker& marking_progress_tracker() {
    return marking_progress_tracker_;
  }
  const MarkingProgressTracker& marking_progress_tracker() const {
    return marking_progress_tracker_;
  }

  Space* owner() const { return reinterpret_cast<Space*>(BasePage::owner()); }

  heap::ListNode<MutablePage>& list_node() { return list_node_; }
  const heap::ListNode<MutablePage>& list_node() const { return list_node_; }

  PossiblyEmptyBuckets* possibly_empty_buckets() {
    return &possibly_empty_buckets_;
  }

  // Release memory allocated by the chunk, except that which is needed by
  // read-only space chunks.
  void ReleaseAllocatedMemoryNeededForWritableChunk();

  void IncreaseAllocatedLabSize(size_t bytes) { allocated_lab_size_ += bytes; }
  void DecreaseAllocatedLabSize(size_t bytes) {
    DCHECK_GE(allocated_lab_size_, bytes);
    allocated_lab_size_ -= bytes;
  }
  size_t AllocatedLabSize() const { return allocated_lab_size_; }

  void IncrementAgeInNewSpace() { age_in_new_space_++; }
  void ResetAgeInNewSpace() { age_in_new_space_ = 0; }
  size_t AgeInNewSpace() const { return age_in_new_space_; }

  void ResetAllocationStatistics() {
    BasePage::ResetAllocationStatistics();
    allocated_lab_size_ = 0;
  }

  void ResetAllocationStatisticsForPromotedPage() {
    DCHECK_NE(0, live_bytes());
    allocated_bytes_ = live_bytes();
    wasted_memory_ = area_size() - allocated_bytes_;
    allocated_lab_size_ = 0;
  }

  MarkingBitmap* marking_bitmap() {
    DCHECK(!IsReadOnlyPage());
    return &marking_bitmap_;
  }

  const MarkingBitmap* marking_bitmap() const {
    DCHECK(!IsReadOnlyPage());
    return &marking_bitmap_;
  }

  size_t live_bytes() const {
    return live_byte_count_.load(std::memory_order_relaxed);
  }

  void SetLiveBytes(size_t value) {
    DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                   ::IsAligned(value, kObjectAlignment8GbHeap));
    live_byte_count_.store(value, std::memory_order_relaxed);
  }

  void IncrementLiveBytesAtomically(intptr_t diff) {
    DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                   ::IsAligned(diff, kObjectAlignment8GbHeap));
    live_byte_count_.fetch_add(diff, std::memory_order_relaxed);
  }

  template <AccessMode mode = AccessMode::NON_ATOMIC>
  void ClearLiveness();

  bool IsLivenessClear() const;

 protected:
  MutablePage(Heap* heap, BaseSpace* space, size_t size, Address area_start,
              Address area_end, VirtualMemory reservation, PageSize page_size,
              Executability executability);

  MemoryChunk::MainThreadFlags ComputeInitialFlags(
      Executability executable) const;

  // Release all memory allocated by the chunk. Should be called when memory
  // chunk is about to be freed.
  void ReleaseAllAllocatedMemory();

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  void set_slot_set(SlotSet* slot_set) {
    if (access_mode == AccessMode::ATOMIC) {
      base::AsAtomicPointer::Release_Store(&slot_set_[type], slot_set);
      return;
    }
    slot_set_[type] = slot_set;
  }

  template <RememberedSetType type, AccessMode access_mode = AccessMode::ATOMIC>
  void set_typed_slot_set(TypedSlotSet* typed_slot_set) {
    if (access_mode == AccessMode::ATOMIC) {
      base::AsAtomicPointer::Release_Store(&typed_slot_set_[type],
                                           typed_slot_set);
      return;
    }
    typed_slot_set_[type] = typed_slot_set;
  }

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES] = {nullptr};
  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  TypedSlotSet* typed_slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES] = {nullptr};

  // Used by the marker to keep track of the scanning progress in large objects
  // that have a progress tracker and are scanned in increments and
  // concurrently.
  MarkingProgressTracker marking_progress_tracker_;

  // Count of bytes marked black on page. With sticky mark-bits, the counter
  // represents the size of the old objects allocated on the page. This is
  // handy, since this counter is then used when starting sweeping to set the
  // approximate allocated size on the space (before it gets refined due to
  // right/left-trimming or slack tracking).
  std::atomic<intptr_t> live_byte_count_{0};

  std::atomic<ConcurrentSweepingState> concurrent_sweeping_{
      ConcurrentSweepingState::kDone};

  heap::ListNode<MutablePage> list_node_;

  FreeListCategory** categories_ = nullptr;

  PossiblyEmptyBuckets possibly_empty_buckets_;

  // This also serves as indicator for whether a page is large. See constructor.
  std::unique_ptr<ActiveSystemPages> active_system_pages_;

  // Counts overall allocated LAB size on the page since the last GC. Used
  // only for new space pages.
  size_t allocated_lab_size_ = 0;

  // Counts the number of young gen GCs that a page survived in new space. This
  // counter is reset to 0 whenever the page is empty.
  size_t age_in_new_space_ = 0;

  MemoryChunk::MainThreadFlags trusted_main_thread_flags_ =
      MemoryChunk::Flag::NO_FLAGS;

  MarkingBitmap marking_bitmap_;

  // Possibly platform-dependent fields should go last. We depend on the marking
  // bitmap offset from generated code and assume that it's stable across 64-bit
  // platforms. In theory, there could be a difference between Linux and Android
  // in terms of Mutex size.

  base::Mutex mutex_;
  base::Mutex object_mutex_;

 private:
  V8_INLINE void RawSetTrustedAndUntrustedFlags(
      MemoryChunk::MainThreadFlags new_flags);
  V8_INLINE void SetFlagsUnlocked(
      MemoryChunk::MainThreadFlags flags,
      MemoryChunk::MainThreadFlags mask = MemoryChunk::kAllFlagsMask);
  V8_INLINE void ClearFlagsUnlocked(MemoryChunk::MainThreadFlags flags);

  static constexpr intptr_t MarkingBitmapOffset() {
    return offsetof(MutablePage, marking_bitmap_);
  }

  static constexpr intptr_t SlotSetOffset(
      RememberedSetType remembered_set_type) {
    return offsetof(MutablePage, slot_set_) +
           sizeof(void*) * remembered_set_type;
  }

  // For ReleaseAllAllocatedMemory().
  friend class MemoryAllocator;
  friend class MemoryPool;
  // For set_typed_slot_set().
  template <RememberedSetType>
  friend class RememberedSet;
  // For MarkingBitmapOffset().
  friend class CodeStubAssembler;
  friend class MacroAssembler;
  friend class MarkingBitmap;
  friend class TestWithBitmap;
  // For SlotSetOffset().
  friend class WriteBarrierCodeStubAssembler;
};

template <>
struct CastTraits<MutablePage> {
  static inline bool AllowFrom(const BasePage& page) {
    return page.IsMutablePage();
  }
};

}  // namespace internal

namespace base {
// Define special hash function for chunk pointers, to be used with std data
// structures, e.g. std::unordered_set<MutablePage*, base::hash<MutablePage*>
template <>
struct hash<i::MutablePage*> : hash<i::BasePage*> {};
template <>
struct hash<const i::MutablePage*> : hash<const i::BasePage*> {};
}  // namespace base

}  // namespace v8

#endif  // V8_HEAP_MUTABLE_PAGE_H_
