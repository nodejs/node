// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk.h"

#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/code-object-registry.h"
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
  DCHECK(owner_identity() == CODE_SPACE || owner_identity() == CODE_LO_SPACE);
  // Decrementing the write_unprotect_counter_ and changing the page
  // protection mode has to be atomic.
  base::MutexGuard guard(page_protection_change_mutex_);
  if (write_unprotect_counter_ == 0) {
    // This is a corner case that may happen when we have a
    // CodeSpaceMemoryModificationScope open and this page was newly
    // added.
    return;
  }
  write_unprotect_counter_--;
  if (write_unprotect_counter_ == 0) {
    Address protect_start =
        address() + MemoryChunkLayout::ObjectStartOffsetInCodePage();
    size_t page_size = MemoryAllocator::GetCommitPageSize();
    DCHECK(IsAligned(protect_start, page_size));
    size_t protect_size = RoundUp(area_size(), page_size);
    CHECK(reservation_.SetPermissions(protect_start, protect_size, permission));
  }
}

void MemoryChunk::SetReadable() {
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(PageAllocator::kRead);
}

void MemoryChunk::SetReadAndExecutable() {
  DCHECK(!v8_flags.jitless);
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::kReadExecute);
}

void MemoryChunk::SetCodeModificationPermissions() {
  DCHECK(!V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT);
  DCHECK(IsFlagSet(MemoryChunk::IS_EXECUTABLE));
  DCHECK(owner_identity() == CODE_SPACE || owner_identity() == CODE_LO_SPACE);
  // Incrementing the write_unprotect_counter_ and changing the page
  // protection mode has to be atomic.
  base::MutexGuard guard(page_protection_change_mutex_);
  write_unprotect_counter_++;
  if (write_unprotect_counter_ == 1) {
    Address unprotect_start =
        address() + MemoryChunkLayout::ObjectStartOffsetInCodePage();
    size_t page_size = MemoryAllocator::GetCommitPageSize();
    DCHECK(IsAligned(unprotect_start, page_size));
    size_t unprotect_size = RoundUp(area_size(), page_size);
    // We may use RWX pages to write code. Some CPUs have optimisations to push
    // updates to code to the icache through a fast path, and they may filter
    // updates based on the written memory being executable.
    CHECK(reservation_.SetPermissions(
        unprotect_start, unprotect_size,
        MemoryChunk::GetCodeModificationPermission()));
  }
}

void MemoryChunk::SetDefaultCodePermissions() {
  if (v8_flags.jitless) {
    SetReadable();
  } else {
    SetReadAndExecutable();
  }
}

namespace {

PageAllocator::Permission DefaultWritableCodePermissions() {
  DCHECK(!V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT);
  // On MacOS on ARM64 RWX permissions are allowed to be set only when
  // fast W^X is enabled (see V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT).
  return V8_HAS_PTHREAD_JIT_WRITE_PROTECT || v8_flags.jitless
             ? PageAllocator::kReadWrite
             : PageAllocator::kReadWriteExecute;
}

}  // namespace

MemoryChunk::MemoryChunk(Heap* heap, BaseSpace* space, size_t chunk_size,
                         Address area_start, Address area_end,
                         VirtualMemory reservation, Executability executable,
                         PageSize page_size)
    : BasicMemoryChunk(heap, space, chunk_size, area_start, area_end,
                       std::move(reservation)) {
  base::AsAtomicPointer::Release_Store(&slot_set_[OLD_TO_NEW], nullptr);
  base::AsAtomicPointer::Release_Store(&slot_set_[OLD_TO_OLD], nullptr);
  base::AsAtomicPointer::Release_Store(&slot_set_[OLD_TO_SHARED], nullptr);
  base::AsAtomicPointer::Release_Store(&slot_set_[OLD_TO_CODE], nullptr);
  base::AsAtomicPointer::Release_Store(&typed_slot_set_[OLD_TO_NEW], nullptr);
  base::AsAtomicPointer::Release_Store(&typed_slot_set_[OLD_TO_OLD], nullptr);
  base::AsAtomicPointer::Release_Store(&typed_slot_set_[OLD_TO_SHARED],
                                       nullptr);
  invalidated_slots_[OLD_TO_NEW] = nullptr;
  invalidated_slots_[OLD_TO_OLD] = nullptr;
  invalidated_slots_[OLD_TO_CODE] = nullptr;
  invalidated_slots_[OLD_TO_SHARED] = nullptr;
  progress_bar_.Initialize();
  set_concurrent_sweeping_state(ConcurrentSweepingState::kDone);
  page_protection_change_mutex_ = new base::Mutex();
  write_unprotect_counter_ = 0;
  mutex_ = new base::Mutex();
  shared_mutex_ = new base::SharedMutex();

  external_backing_store_bytes_[ExternalBackingStoreType::kArrayBuffer] = 0;
  external_backing_store_bytes_[ExternalBackingStoreType::kExternalString] = 0;

  categories_ = nullptr;

  heap->non_atomic_marking_state()->SetLiveBytes(this, 0);
  if (executable == EXECUTABLE) {
    SetFlag(IS_EXECUTABLE);
    if (heap->write_protect_code_memory()) {
      write_unprotect_counter_ =
          heap->code_space_memory_modification_scope_depth();
    } else if (!V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT) {
      size_t page_size = MemoryAllocator::GetCommitPageSize();
      // On executable chunks, area_start_ points past padding used for code
      // alignment.
      Address start_before_padding =
          address() + MemoryChunkLayout::ObjectPageOffsetInCodePage();
      DCHECK(IsAligned(start_before_padding, page_size));
      size_t area_size = RoundUp(area_end_ - start_before_padding, page_size);
      CHECK(reservation_.SetPermissions(start_before_padding, area_size,
                                        DefaultWritableCodePermissions()));
    }
  }

  if (owner()->identity() == CODE_SPACE) {
    code_object_registry_ = new CodeObjectRegistry();
  } else {
    code_object_registry_ = nullptr;
  }

  possibly_empty_buckets_.Initialize();

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

void MemoryChunk::SetOldGenerationPageFlags(bool is_marking) {
  if (is_marking) {
    SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else {
    if (owner_identity() == SHARED_SPACE ||
        owner_identity() == SHARED_LO_SPACE) {
      // We need to track pointers into the SHARED_SPACE for OLD_TO_SHARED.
      SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
      // No need to track OLD_TO_NEW or OLD_TO_SHARED within the shared space.
      ClearFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    } else {
      ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
      SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    }
    ClearFlag(MemoryChunk::INCREMENTAL_MARKING);
  }
}

void MemoryChunk::SetYoungGenerationPageFlags(bool is_marking) {
  SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
  if (is_marking) {
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
  if (code_object_registry_ != nullptr) {
    delete code_object_registry_;
    code_object_registry_ = nullptr;
  }

  if (active_system_pages_ != nullptr) {
    delete active_system_pages_;
    active_system_pages_ = nullptr;
  }

  possibly_empty_buckets_.Release();
  ReleaseSlotSet<OLD_TO_NEW>();
  ReleaseSlotSet<OLD_TO_OLD>();
  ReleaseSlotSet<OLD_TO_CODE>();
  ReleaseSlotSet<OLD_TO_SHARED>();
  ReleaseTypedSlotSet<OLD_TO_NEW>();
  ReleaseTypedSlotSet<OLD_TO_OLD>();
  ReleaseTypedSlotSet<OLD_TO_SHARED>();
  ReleaseInvalidatedSlots<OLD_TO_NEW>();
  ReleaseInvalidatedSlots<OLD_TO_OLD>();
  ReleaseInvalidatedSlots<OLD_TO_SHARED>();

  if (!IsLargePage()) {
    Page* page = static_cast<Page*>(this);
    page->ReleaseFreeListCategories();
  }
}

void MemoryChunk::ReleaseAllAllocatedMemory() {
  ReleaseAllocatedMemoryNeededForWritableChunk();
}

template V8_EXPORT_PRIVATE SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_NEW>();
template V8_EXPORT_PRIVATE SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_OLD>();
template V8_EXPORT_PRIVATE SlotSet*
MemoryChunk::AllocateSlotSet<OLD_TO_SHARED>();
template V8_EXPORT_PRIVATE SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_CODE>();

template <RememberedSetType type>
SlotSet* MemoryChunk::AllocateSlotSet() {
  return AllocateSlotSet(&slot_set_[type]);
}

SlotSet* MemoryChunk::AllocateSlotSet(SlotSet** slot_set) {
  SlotSet* new_slot_set = SlotSet::Allocate(buckets());
  SlotSet* old_slot_set = base::AsAtomicPointer::AcquireRelease_CompareAndSwap(
      slot_set, nullptr, new_slot_set);
  if (old_slot_set != nullptr) {
    SlotSet::Delete(new_slot_set, buckets());
    new_slot_set = old_slot_set;
  }
  DCHECK(new_slot_set);
  return new_slot_set;
}

template void MemoryChunk::ReleaseSlotSet<OLD_TO_NEW>();
template void MemoryChunk::ReleaseSlotSet<OLD_TO_OLD>();
template void MemoryChunk::ReleaseSlotSet<OLD_TO_SHARED>();
template void MemoryChunk::ReleaseSlotSet<OLD_TO_CODE>();

template <RememberedSetType type>
void MemoryChunk::ReleaseSlotSet() {
  ReleaseSlotSet(&slot_set_[type]);
}

void MemoryChunk::ReleaseSlotSet(SlotSet** slot_set) {
  if (*slot_set) {
    SlotSet::Delete(*slot_set, buckets());
    *slot_set = nullptr;
  }
}

template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_NEW>();
template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_OLD>();
template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_SHARED>();

template <RememberedSetType type>
TypedSlotSet* MemoryChunk::AllocateTypedSlotSet() {
  TypedSlotSet* typed_slot_set = new TypedSlotSet(address());
  TypedSlotSet* old_value = base::AsAtomicPointer::Release_CompareAndSwap(
      &typed_slot_set_[type], nullptr, typed_slot_set);
  if (old_value != nullptr) {
    delete typed_slot_set;
    typed_slot_set = old_value;
  }
  DCHECK(typed_slot_set);
  return typed_slot_set;
}

template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_NEW>();
template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_OLD>();
template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_SHARED>();

template <RememberedSetType type>
void MemoryChunk::ReleaseTypedSlotSet() {
  TypedSlotSet* typed_slot_set = typed_slot_set_[type];
  if (typed_slot_set) {
    typed_slot_set_[type] = nullptr;
    delete typed_slot_set;
  }
}

template InvalidatedSlots* MemoryChunk::AllocateInvalidatedSlots<OLD_TO_NEW>();
template InvalidatedSlots* MemoryChunk::AllocateInvalidatedSlots<OLD_TO_OLD>();

template <RememberedSetType type>
InvalidatedSlots* MemoryChunk::AllocateInvalidatedSlots() {
  DCHECK_NULL(invalidated_slots_[type]);
  invalidated_slots_[type] = new InvalidatedSlots();
  return invalidated_slots_[type];
}

template void MemoryChunk::ReleaseInvalidatedSlots<OLD_TO_NEW>();
template void MemoryChunk::ReleaseInvalidatedSlots<OLD_TO_OLD>();
template void MemoryChunk::ReleaseInvalidatedSlots<OLD_TO_SHARED>();

template <RememberedSetType type>
void MemoryChunk::ReleaseInvalidatedSlots() {
  if (invalidated_slots_[type]) {
    delete invalidated_slots_[type];
    invalidated_slots_[type] = nullptr;
  }
}

template V8_EXPORT_PRIVATE void
MemoryChunk::RegisterObjectWithInvalidatedSlots<OLD_TO_NEW>(HeapObject object,
                                                            int new_size);
template V8_EXPORT_PRIVATE void
MemoryChunk::RegisterObjectWithInvalidatedSlots<OLD_TO_OLD>(HeapObject object,
                                                            int new_size);
template V8_EXPORT_PRIVATE void MemoryChunk::RegisterObjectWithInvalidatedSlots<
    OLD_TO_SHARED>(HeapObject object, int new_size);

template <RememberedSetType type>
void MemoryChunk::RegisterObjectWithInvalidatedSlots(HeapObject object,
                                                     int new_size) {
  // ByteArray and FixedArray are still invalidated in tests.
  DCHECK(object.IsString() || object.IsByteArray() || object.IsFixedArray());
  DCHECK(!object.InWritableSharedSpace());
  bool skip_slot_recording;

  switch (type) {
    case OLD_TO_NEW:
      skip_slot_recording = InYoungGeneration();
      break;

    case OLD_TO_OLD:
      skip_slot_recording = ShouldSkipEvacuationSlotRecording();
      break;

    case OLD_TO_SHARED:
      skip_slot_recording = InYoungGeneration();
      break;

    default:
      UNREACHABLE();
  }

  if (skip_slot_recording) {
    return;
  }

  if (invalidated_slots<type>() == nullptr) {
    AllocateInvalidatedSlots<type>();
  }

  DCHECK_GT(new_size, 0);
  InvalidatedSlots& invalidated_slots = *this->invalidated_slots<type>();
  DCHECK_IMPLIES(invalidated_slots.count(object) > 0,
                 new_size <= invalidated_slots[object]);
  invalidated_slots.insert_or_assign(object, new_size);
}

template V8_EXPORT_PRIVATE void
MemoryChunk::UpdateInvalidatedObjectSize<OLD_TO_NEW>(HeapObject object,
                                                     int new_size);
template V8_EXPORT_PRIVATE void
MemoryChunk::UpdateInvalidatedObjectSize<OLD_TO_OLD>(HeapObject object,
                                                     int new_size);
template V8_EXPORT_PRIVATE void
MemoryChunk::UpdateInvalidatedObjectSize<OLD_TO_SHARED>(HeapObject object,
                                                        int new_size);

template <RememberedSetType type>
void MemoryChunk::UpdateInvalidatedObjectSize(HeapObject object, int new_size) {
  DCHECK(!object.InWritableSharedSpace());
  DCHECK_GT(new_size, 0);

  if (invalidated_slots<type>() == nullptr) return;

  InvalidatedSlots& invalidated_slots = *this->invalidated_slots<type>();
  if (invalidated_slots.count(object) > 0) {
    DCHECK_LE(new_size, invalidated_slots[object]);
    DCHECK_NE(0, invalidated_slots[object]);
    invalidated_slots.insert_or_assign(object, new_size);
  }
}

template bool MemoryChunk::RegisteredObjectWithInvalidatedSlots<OLD_TO_NEW>(
    HeapObject object);
template bool MemoryChunk::RegisteredObjectWithInvalidatedSlots<OLD_TO_OLD>(
    HeapObject object);
template bool MemoryChunk::RegisteredObjectWithInvalidatedSlots<OLD_TO_SHARED>(
    HeapObject object);

template <RememberedSetType type>
bool MemoryChunk::RegisteredObjectWithInvalidatedSlots(HeapObject object) {
  if (invalidated_slots<type>() == nullptr) {
    return false;
  }
  return invalidated_slots<type>()->find(object) !=
         invalidated_slots<type>()->end();
}

bool MemoryChunk::HasRecordedSlots() const {
  for (int rs_type = 0; rs_type < NUMBER_OF_REMEMBERED_SET_TYPES; rs_type++) {
    if (slot_set_[rs_type] || typed_slot_set_[rs_type] ||
        invalidated_slots_[rs_type]) {
      return true;
    }
  }

  return false;
}

bool MemoryChunk::HasRecordedOldToNewSlots() const {
  return slot_set_[OLD_TO_NEW] || typed_slot_set_[OLD_TO_NEW] ||
         invalidated_slots_[OLD_TO_NEW];
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
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->invalidated_slots_) - chunk->address(),
      MemoryChunkLayout::kInvalidatedSlotsOffset);
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
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->write_unprotect_counter_) -
                chunk->address(),
            MemoryChunkLayout::kWriteUnprotectCounterOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->external_backing_store_bytes_) -
                chunk->address(),
            MemoryChunkLayout::kExternalBackingStoreBytesOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->list_node_) - chunk->address(),
            MemoryChunkLayout::kListNodeOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->categories_) - chunk->address(),
            MemoryChunkLayout::kCategoriesOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->code_object_registry_) -
                chunk->address(),
            MemoryChunkLayout::kCodeObjectRegistryOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->possibly_empty_buckets_) -
                chunk->address(),
            MemoryChunkLayout::kPossiblyEmptyBucketsOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->active_system_pages_) -
                chunk->address(),
            MemoryChunkLayout::kActiveSystemPagesOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->was_used_for_allocation_) -
                chunk->address(),
            MemoryChunkLayout::kWasUsedForAllocationOffset);
}
#endif

}  // namespace internal
}  // namespace v8
