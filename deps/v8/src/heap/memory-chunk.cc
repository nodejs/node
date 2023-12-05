// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk.h"

#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

void MemoryChunk::DiscardUnusedMemory(Address addr, size_t size) {
  base::AddressRegion memory_area =
      MemoryAllocator::ComputeDiscardMemoryArea(addr, size);
  if (memory_area.size() != 0) {
    MemoryAllocator* memory_allocator = heap_->memory_allocator();
    v8::PageAllocator* page_allocator =
        memory_allocator->page_allocator(executable());
    CHECK(page_allocator->DiscardSystemPages(
        reinterpret_cast<void*>(memory_area.begin()), memory_area.size()));
  }
}

void MemoryChunk::InitializationMemoryFence() {
  base::SeqCst_MemoryFence();
#ifdef THREAD_SANITIZER
  // Since TSAN does not process memory fences, we use the following annotation
  // to tell TSAN that there is no data race when emitting a
  // InitializationMemoryFence. Note that the other thread still needs to
  // perform MemoryChunk::synchronized_heap().
  base::Release_Store(reinterpret_cast<base::AtomicWord*>(&heap_),
                      reinterpret_cast<base::AtomicWord>(heap_));
#endif
}

void MemoryChunk::DecrementWriteUnprotectCounterAndMaybeSetPermissions(
    PageAllocator::Permission permission) {
  DCHECK(!V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT);
  DCHECK(permission == PageAllocator::kRead ||
         permission == PageAllocator::kReadExecute);
  DCHECK(IsFlagSet(MemoryChunk::IS_EXECUTABLE));
  DCHECK(IsAnyCodeSpace(owner_identity()));
  page_protection_change_mutex_->AssertHeld();
  Address protect_start =
      address() + MemoryChunkLayout::ObjectPageOffsetInCodePage();
  size_t page_size = MemoryAllocator::GetCommitPageSize();
  DCHECK(IsAligned(protect_start, page_size));
  size_t protect_size = RoundUp(area_size(), page_size);
  CHECK(reservation_.SetPermissions(protect_start, protect_size, permission));
}

void MemoryChunk::SetReadable() {
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(PageAllocator::kRead);
}

void MemoryChunk::SetReadAndExecutable() {
  DCHECK(!v8_flags.jitless);
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::kReadExecute);
}

base::MutexGuard MemoryChunk::SetCodeModificationPermissions() {
  DCHECK(!V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT);
  DCHECK(IsFlagSet(MemoryChunk::IS_EXECUTABLE));
  DCHECK(IsAnyCodeSpace(owner_identity()));
  // Incrementing the write_unprotect_counter_ and changing the page
  // protection mode has to be atomic.
  base::MutexGuard guard(page_protection_change_mutex_);

  Address unprotect_start =
      address() + MemoryChunkLayout::ObjectPageOffsetInCodePage();
  size_t page_size = MemoryAllocator::GetCommitPageSize();
  DCHECK(IsAligned(unprotect_start, page_size));
  size_t unprotect_size = RoundUp(area_size(), page_size);
  // We may use RWX pages to write code. Some CPUs have optimisations to push
  // updates to code to the icache through a fast path, and they may filter
  // updates based on the written memory being executable.
  CHECK(reservation_.SetPermissions(
      unprotect_start, unprotect_size,
      MemoryChunk::GetCodeModificationPermission()));

  return guard;
}

void MemoryChunk::SetDefaultCodePermissions() {
  if (v8_flags.jitless) {
    SetReadable();
  } else {
    SetReadAndExecutable();
  }
}

MemoryChunk::MemoryChunk(Heap* heap, BaseSpace* space, size_t chunk_size,
                         Address area_start, Address area_end,
                         VirtualMemory reservation, Executability executable,
                         PageSize page_size)
    : BasicMemoryChunk(heap, space, chunk_size, area_start, area_end,
                       std::move(reservation)),
      mutex_(new base::Mutex()),
      shared_mutex_(new base::SharedMutex()),
      page_protection_change_mutex_(new base::Mutex()) {
  DCHECK_NE(space->identity(), RO_SPACE);

  if (executable == EXECUTABLE) {
    SetFlag(IS_EXECUTABLE);
  }

  if (page_size == PageSize::kRegular) {
    active_system_pages_ = new ActiveSystemPages;
    active_system_pages_->Init(MemoryChunkLayout::kMemoryChunkHeaderSize,
                               MemoryAllocator::GetCommitPageSizeBits(),
                               size());
  } else {
    // We do not track active system pages for large pages.
    active_system_pages_ = nullptr;
  }

  // All pages of a shared heap need to be marked with this flag.
  if (owner()->identity() == SHARED_SPACE ||
      owner()->identity() == SHARED_LO_SPACE) {
    SetFlag(MemoryChunk::IN_WRITABLE_SHARED_SPACE);
  }

#ifdef DEBUG
  ValidateOffsets(this);
#endif
}

size_t MemoryChunk::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits() || IsLargePage()) return size();
  return active_system_pages_->Size(MemoryAllocator::GetCommitPageSizeBits());
}

void MemoryChunk::SetOldGenerationPageFlags(MarkingMode marking_mode) {
  if (marking_mode == MarkingMode::kMajorMarking) {
    SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else if (owner_identity() == SHARED_SPACE ||
             owner_identity() == SHARED_LO_SPACE) {
    // We need to track pointers into the SHARED_SPACE for OLD_TO_SHARED.
    SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    // No need to track OLD_TO_NEW or OLD_TO_SHARED within the shared space.
    ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    ClearFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else {
    ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    if (marking_mode == MarkingMode::kMinorMarking) {
      SetFlag(MemoryChunk::INCREMENTAL_MARKING);
    } else {
      ClearFlags(MemoryChunk::INCREMENTAL_MARKING);
    }
  }
}

void MemoryChunk::SetYoungGenerationPageFlags(MarkingMode marking_mode) {
  SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
  if (marking_mode != MarkingMode::kNoMarking) {
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else {
    ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    ClearFlag(MemoryChunk::INCREMENTAL_MARKING);
  }
}
// -----------------------------------------------------------------------------
// MemoryChunk implementation

void MemoryChunk::ReleaseAllocatedMemoryNeededForWritableChunk() {
  DCHECK(SweepingDone());
  if (mutex_ != nullptr) {
    delete mutex_;
    mutex_ = nullptr;
  }
  if (shared_mutex_) {
    delete shared_mutex_;
    shared_mutex_ = nullptr;
  }
  if (page_protection_change_mutex_ != nullptr) {
    delete page_protection_change_mutex_;
    page_protection_change_mutex_ = nullptr;
  }

  if (active_system_pages_ != nullptr) {
    delete active_system_pages_;
    active_system_pages_ = nullptr;
  }

  possibly_empty_buckets_.Release();
  ReleaseSlotSet(OLD_TO_NEW);
  ReleaseSlotSet(OLD_TO_NEW_BACKGROUND);
  ReleaseSlotSet(OLD_TO_OLD);
  ReleaseSlotSet(OLD_TO_CODE);
  ReleaseSlotSet(OLD_TO_SHARED);
  ReleaseTypedSlotSet(OLD_TO_NEW);
  ReleaseTypedSlotSet(OLD_TO_OLD);
  ReleaseTypedSlotSet(OLD_TO_SHARED);

  if (!IsLargePage()) {
    Page* page = static_cast<Page*>(this);
    page->ReleaseFreeListCategories();
  }
}

void MemoryChunk::ReleaseAllAllocatedMemory() {
  ReleaseAllocatedMemoryNeededForWritableChunk();
}

SlotSet* MemoryChunk::AllocateSlotSet(RememberedSetType type) {
  SlotSet* new_slot_set = SlotSet::Allocate(buckets());
  SlotSet* old_slot_set = base::AsAtomicPointer::AcquireRelease_CompareAndSwap(
      &slot_set_[type], nullptr, new_slot_set);
  if (old_slot_set) {
    SlotSet::Delete(new_slot_set, buckets());
    new_slot_set = old_slot_set;
  }
  DCHECK_NOT_NULL(new_slot_set);
  return new_slot_set;
}

void MemoryChunk::ReleaseSlotSet(RememberedSetType type) {
  SlotSet* slot_set = slot_set_[type];
  if (slot_set) {
    slot_set_[type] = nullptr;
    SlotSet::Delete(slot_set, buckets());
  }
}

TypedSlotSet* MemoryChunk::AllocateTypedSlotSet(RememberedSetType type) {
  TypedSlotSet* typed_slot_set = new TypedSlotSet(address());
  TypedSlotSet* old_value = base::AsAtomicPointer::Release_CompareAndSwap(
      &typed_slot_set_[type], nullptr, typed_slot_set);
  if (old_value) {
    delete typed_slot_set;
    typed_slot_set = old_value;
  }
  DCHECK(typed_slot_set);
  return typed_slot_set;
}

void MemoryChunk::ReleaseTypedSlotSet(RememberedSetType type) {
  TypedSlotSet* typed_slot_set = typed_slot_set_[type];
  if (typed_slot_set) {
    typed_slot_set_[type] = nullptr;
    delete typed_slot_set;
  }
}

bool MemoryChunk::ContainsAnySlots() const {
  for (int rs_type = 0; rs_type < NUMBER_OF_REMEMBERED_SET_TYPES; rs_type++) {
    if (slot_set_[rs_type] || typed_slot_set_[rs_type]) {
      return true;
    }
  }
  return false;
}

void MemoryChunk::ClearLiveness() {
  marking_bitmap()->Clear<AccessMode::NON_ATOMIC>();
  SetLiveBytes(0);
}

#ifdef DEBUG
void MemoryChunk::ValidateOffsets(MemoryChunk* chunk) {
  // Note that we cannot use offsetof because MemoryChunk is not a POD.
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->slot_set_) - chunk->address(),
            MemoryChunkLayout::kSlotSetOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->progress_bar_) - chunk->address(),
            MemoryChunkLayout::kProgressBarOffset);
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->live_byte_count_) - chunk->address(),
      MemoryChunkLayout::kLiveByteCountOffset);
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->typed_slot_set_) - chunk->address(),
      MemoryChunkLayout::kTypedSlotSetOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->mutex_) - chunk->address(),
            MemoryChunkLayout::kMutexOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->shared_mutex_) - chunk->address(),
            MemoryChunkLayout::kSharedMutexOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->concurrent_sweeping_) -
                chunk->address(),
            MemoryChunkLayout::kConcurrentSweepingOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->page_protection_change_mutex_) -
                chunk->address(),
            MemoryChunkLayout::kPageProtectionChangeMutexOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->external_backing_store_bytes_) -
                chunk->address(),
            MemoryChunkLayout::kExternalBackingStoreBytesOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->list_node_) - chunk->address(),
            MemoryChunkLayout::kListNodeOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->categories_) - chunk->address(),
            MemoryChunkLayout::kCategoriesOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->possibly_empty_buckets_) -
                chunk->address(),
            MemoryChunkLayout::kPossiblyEmptyBucketsOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->active_system_pages_) -
                chunk->address(),
            MemoryChunkLayout::kActiveSystemPagesOffset);
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->allocated_lab_size_) - chunk->address(),
      MemoryChunkLayout::kAllocatedLabSizeOffset);
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->age_in_new_space_) - chunk->address(),
      MemoryChunkLayout::kAgeInNewSpaceOffset);
}
#endif

}  // namespace internal
}  // namespace v8
