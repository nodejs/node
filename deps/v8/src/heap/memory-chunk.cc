// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk.h"

#include "src/base/platform/platform.h"
#include "src/heap/code-object-registry.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
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
  DCHECK_LT(write_unprotect_counter_, kMaxWriteUnprotectCounter);
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
  DCHECK(!FLAG_jitless);
  DecrementWriteUnprotectCounterAndMaybeSetPermissions(
      PageAllocator::kReadExecute);
}

void MemoryChunk::SetReadAndWritable() {
  DCHECK(IsFlagSet(MemoryChunk::IS_EXECUTABLE));
  DCHECK(owner_identity() == CODE_SPACE || owner_identity() == CODE_LO_SPACE);
  // Incrementing the write_unprotect_counter_ and changing the page
  // protection mode has to be atomic.
  base::MutexGuard guard(page_protection_change_mutex_);
  write_unprotect_counter_++;
  DCHECK_LE(write_unprotect_counter_, kMaxWriteUnprotectCounter);
  if (write_unprotect_counter_ == 1) {
    Address unprotect_start =
        address() + MemoryChunkLayout::ObjectStartOffsetInCodePage();
    size_t page_size = MemoryAllocator::GetCommitPageSize();
    DCHECK(IsAligned(unprotect_start, page_size));
    size_t unprotect_size = RoundUp(area_size(), page_size);
    CHECK(reservation_.SetPermissions(unprotect_start, unprotect_size,
                                      PageAllocator::kReadWrite));
  }
}

namespace {

PageAllocator::Permission DefaultWritableCodePermissions() {
  return FLAG_jitless ? PageAllocator::kReadWrite
                      : PageAllocator::kReadWriteExecute;
}

}  // namespace

MemoryChunk* MemoryChunk::Initialize(BasicMemoryChunk* basic_chunk, Heap* heap,
                                     Executability executable) {
  MemoryChunk* chunk = static_cast<MemoryChunk*>(basic_chunk);

  base::AsAtomicPointer::Release_Store(&chunk->slot_set_[OLD_TO_NEW], nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->slot_set_[OLD_TO_OLD], nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->sweeping_slot_set_, nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->typed_slot_set_[OLD_TO_NEW],
                                       nullptr);
  base::AsAtomicPointer::Release_Store(&chunk->typed_slot_set_[OLD_TO_OLD],
                                       nullptr);
  chunk->invalidated_slots_[OLD_TO_NEW] = nullptr;
  chunk->invalidated_slots_[OLD_TO_OLD] = nullptr;
  chunk->progress_bar_ = 0;
  chunk->set_concurrent_sweeping_state(ConcurrentSweepingState::kDone);
  chunk->page_protection_change_mutex_ = new base::Mutex();
  chunk->write_unprotect_counter_ = 0;
  chunk->mutex_ = new base::Mutex();
  chunk->young_generation_bitmap_ = nullptr;

  chunk->external_backing_store_bytes_[ExternalBackingStoreType::kArrayBuffer] =
      0;
  chunk->external_backing_store_bytes_
      [ExternalBackingStoreType::kExternalString] = 0;

  chunk->categories_ = nullptr;

  heap->incremental_marking()->non_atomic_marking_state()->SetLiveBytes(chunk,
                                                                        0);
  if (executable == EXECUTABLE) {
    chunk->SetFlag(IS_EXECUTABLE);
    if (heap->write_protect_code_memory()) {
      chunk->write_unprotect_counter_ =
          heap->code_space_memory_modification_scope_depth();
    } else {
      size_t page_size = MemoryAllocator::GetCommitPageSize();
      DCHECK(IsAligned(chunk->area_start(), page_size));
      size_t area_size =
          RoundUp(chunk->area_end() - chunk->area_start(), page_size);
      CHECK(chunk->reservation_.SetPermissions(
          chunk->area_start(), area_size, DefaultWritableCodePermissions()));
    }
  }

  if (chunk->owner()->identity() == CODE_SPACE) {
    chunk->code_object_registry_ = new CodeObjectRegistry();
  } else {
    chunk->code_object_registry_ = nullptr;
  }

  chunk->possibly_empty_buckets_.Initialize();

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  chunk->object_start_bitmap_ = ObjectStartBitmap(chunk->area_start());
#endif

#ifdef DEBUG
  ValidateOffsets(chunk);
#endif

  return chunk;
}

size_t MemoryChunk::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits() || owner_identity() == LO_SPACE)
    return size();
  return high_water_mark_;
}

void MemoryChunk::SetOldGenerationPageFlags(bool is_marking) {
  if (is_marking) {
    SetFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  } else {
    ClearFlag(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING);
    SetFlag(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);
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
  if (mutex_ != nullptr) {
    delete mutex_;
    mutex_ = nullptr;
  }
  if (page_protection_change_mutex_ != nullptr) {
    delete page_protection_change_mutex_;
    page_protection_change_mutex_ = nullptr;
  }
  if (code_object_registry_ != nullptr) {
    delete code_object_registry_;
    code_object_registry_ = nullptr;
  }

  possibly_empty_buckets_.Release();
  ReleaseSlotSet<OLD_TO_NEW>();
  ReleaseSweepingSlotSet();
  ReleaseSlotSet<OLD_TO_OLD>();
  ReleaseTypedSlotSet<OLD_TO_NEW>();
  ReleaseTypedSlotSet<OLD_TO_OLD>();
  ReleaseInvalidatedSlots<OLD_TO_NEW>();
  ReleaseInvalidatedSlots<OLD_TO_OLD>();

  if (young_generation_bitmap_ != nullptr) ReleaseYoungGenerationBitmap();

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

template <RememberedSetType type>
SlotSet* MemoryChunk::AllocateSlotSet() {
  return AllocateSlotSet(&slot_set_[type]);
}

SlotSet* MemoryChunk::AllocateSweepingSlotSet() {
  return AllocateSlotSet(&sweeping_slot_set_);
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

template <RememberedSetType type>
void MemoryChunk::ReleaseSlotSet() {
  ReleaseSlotSet(&slot_set_[type]);
}

void MemoryChunk::ReleaseSweepingSlotSet() {
  ReleaseSlotSet(&sweeping_slot_set_);
}

void MemoryChunk::ReleaseSlotSet(SlotSet** slot_set) {
  if (*slot_set) {
    SlotSet::Delete(*slot_set, buckets());
    *slot_set = nullptr;
  }
}

template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_NEW>();
template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_OLD>();

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

template <RememberedSetType type>
void MemoryChunk::ReleaseInvalidatedSlots() {
  if (invalidated_slots_[type]) {
    delete invalidated_slots_[type];
    invalidated_slots_[type] = nullptr;
  }
}

template V8_EXPORT_PRIVATE void
MemoryChunk::RegisterObjectWithInvalidatedSlots<OLD_TO_NEW>(HeapObject object);
template V8_EXPORT_PRIVATE void
MemoryChunk::RegisterObjectWithInvalidatedSlots<OLD_TO_OLD>(HeapObject object);

template <RememberedSetType type>
void MemoryChunk::RegisterObjectWithInvalidatedSlots(HeapObject object) {
  bool skip_slot_recording;

  if (type == OLD_TO_NEW) {
    skip_slot_recording = InYoungGeneration();
  } else {
    skip_slot_recording = ShouldSkipEvacuationSlotRecording();
  }

  if (skip_slot_recording) {
    return;
  }

  if (invalidated_slots<type>() == nullptr) {
    AllocateInvalidatedSlots<type>();
  }

  invalidated_slots<type>()->insert(object);
}

void MemoryChunk::InvalidateRecordedSlots(HeapObject object) {
  if (V8_DISABLE_WRITE_BARRIERS_BOOL) return;
  if (heap()->incremental_marking()->IsCompacting()) {
    // We cannot check slot_set_[OLD_TO_OLD] here, since the
    // concurrent markers might insert slots concurrently.
    RegisterObjectWithInvalidatedSlots<OLD_TO_OLD>(object);
  }

  if (!FLAG_always_promote_young_mc || slot_set_[OLD_TO_NEW] != nullptr)
    RegisterObjectWithInvalidatedSlots<OLD_TO_NEW>(object);
}

template bool MemoryChunk::RegisteredObjectWithInvalidatedSlots<OLD_TO_NEW>(
    HeapObject object);
template bool MemoryChunk::RegisteredObjectWithInvalidatedSlots<OLD_TO_OLD>(
    HeapObject object);

template <RememberedSetType type>
bool MemoryChunk::RegisteredObjectWithInvalidatedSlots(HeapObject object) {
  if (invalidated_slots<type>() == nullptr) {
    return false;
  }
  return invalidated_slots<type>()->find(object) !=
         invalidated_slots<type>()->end();
}

void MemoryChunk::AllocateYoungGenerationBitmap() {
  DCHECK_NULL(young_generation_bitmap_);
  young_generation_bitmap_ = static_cast<Bitmap*>(calloc(1, Bitmap::kSize));
}

void MemoryChunk::ReleaseYoungGenerationBitmap() {
  DCHECK_NOT_NULL(young_generation_bitmap_);
  free(young_generation_bitmap_);
  young_generation_bitmap_ = nullptr;
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
      reinterpret_cast<Address>(&chunk->sweeping_slot_set_) - chunk->address(),
      MemoryChunkLayout::kSweepingSlotSetOffset);
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->typed_slot_set_) - chunk->address(),
      MemoryChunkLayout::kTypedSlotSetOffset);
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->invalidated_slots_) - chunk->address(),
      MemoryChunkLayout::kInvalidatedSlotsOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->mutex_) - chunk->address(),
            MemoryChunkLayout::kMutexOffset);
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
  DCHECK_EQ(
      reinterpret_cast<Address>(&chunk->young_generation_live_byte_count_) -
          chunk->address(),
      MemoryChunkLayout::kYoungGenerationLiveByteCountOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->young_generation_bitmap_) -
                chunk->address(),
            MemoryChunkLayout::kYoungGenerationBitmapOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->code_object_registry_) -
                chunk->address(),
            MemoryChunkLayout::kCodeObjectRegistryOffset);
  DCHECK_EQ(reinterpret_cast<Address>(&chunk->possibly_empty_buckets_) -
                chunk->address(),
            MemoryChunkLayout::kPossiblyEmptyBucketsOffset);
}
#endif

}  // namespace internal
}  // namespace v8
