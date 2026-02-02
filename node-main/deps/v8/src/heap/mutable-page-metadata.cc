// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mutable-page-metadata.h"

#include <new>

#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk-metadata.h"
#include "src/heap/mutable-page-metadata-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"

namespace v8::internal {

MutablePageMetadata::MutablePageMetadata(Heap* heap, BaseSpace* space,
                                         size_t chunk_size, Address area_start,
                                         Address area_end,
                                         VirtualMemory reservation,
                                         PageSize page_size,
                                         Executability executability)
    : MemoryChunkMetadata(heap, space, chunk_size, area_start, area_end,
                          std::move(reservation), executability) {
  DCHECK_NE(space->identity(), RO_SPACE);

  if (page_size == PageSize::kRegular) {
    active_system_pages_ = std::make_unique<ActiveSystemPages>();
    active_system_pages_->Init(
        sizeof(MemoryChunk), MemoryAllocator::GetCommitPageSizeBits(), size());
  }

  // TODO(sroettger): The following fields are accessed most often (AFAICT) and
  // are moved to the end to occupy the same cache line as the slot set array.
  // Without this change, there was a 0.5% performance impact after cache line
  // aligning the metadata on x64 (before, the metadata started at offset 0x10).
  // After reordering, the impact is still 0.1%/0.2% on jetstream2/speedometer3,
  // so there should be some more optimization potential here.
  // TODO(mlippautz): Replace 64 below with
  // `hardware_destructive_interference_size` once supported.
  static constexpr auto kOffsetOfFirstFastField =
      offsetof(MutablePageMetadata, heap_);
  static constexpr auto kOffsetOfLastFastField =
      offsetof(MutablePageMetadata, slot_set_) +
      sizeof(SlotSet*) * RememberedSetType::OLD_TO_NEW;
  // This assert is merely necessary but not sufficient to guarantee that the
  // fields sit on the same cacheline as the metadata object itself is
  // dynamically allocated without alignment restrictions.
  static_assert(kOffsetOfFirstFastField / 64 == kOffsetOfLastFastField / 64);
}

// static
MemoryChunk::MainThreadFlags MutablePageMetadata::OldGenerationPageFlags(
    MarkingMode marking_mode, AllocationSpace space) {
  MemoryChunk::MainThreadFlags flags_to_set = MemoryChunk::NO_FLAGS;

#if V8_ENABLE_STICKY_MARK_BITS_BOOL
  if constexpr (v8_flags.sticky_mark_bits.value()) {
    if (space != OLD_SPACE) {
      flags_to_set |= MemoryChunk::STICKY_MARK_BIT_CONTAINS_ONLY_OLD;
    }
  }
#endif  // V8_ENABLE_STICKY_MARK_BITS_BOOL

  if (marking_mode == MarkingMode::kMajorMarking) {
    flags_to_set |= MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING |
                    MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING |
                    MemoryChunk::INCREMENTAL_MARKING;
#if V8_ENABLE_STICKY_MARK_BITS_BOOL
    flags_to_set |= MemoryChunk::STICKY_MARK_BIT_IS_MAJOR_GC_IN_PROGRESS;
#endif
  } else if (IsAnyWritableSharedSpace(space)) {
    // We need to track pointers into the SHARED_SPACE for OLD_TO_SHARED.
    flags_to_set |= MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING;
  } else {
    flags_to_set |= MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING;
    if (marking_mode == MarkingMode::kMinorMarking) {
      flags_to_set |= MemoryChunk::INCREMENTAL_MARKING;
    }
  }

  return flags_to_set;
}

// static
MemoryChunk::MainThreadFlags MutablePageMetadata::YoungGenerationPageFlags(
    MarkingMode marking_mode) {
  MemoryChunk::MainThreadFlags flags =
      MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING;
  if (marking_mode != MarkingMode::kNoMarking) {
    flags |= MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING;
    flags |= MemoryChunk::INCREMENTAL_MARKING;
#if V8_ENABLE_STICKY_MARK_BITS_BOOL
    if (marking_mode == MarkingMode::kMajorMarking) {
      flags |= MemoryChunk::STICKY_MARK_BIT_IS_MAJOR_GC_IN_PROGRESS;
    }
#endif
  }
  return flags;
}

MemoryChunk::MainThreadFlags MutablePageMetadata::ComputeInitialFlags(
    Executability executable) const {
  const AllocationSpace space = owner()->identity();
  MemoryChunk::MainThreadFlags flags = MemoryChunk::NO_FLAGS;

  if (IsAnyNewSpace(space)) {
    flags |=
        YoungGenerationPageFlags(heap()->incremental_marking()->marking_mode());
  } else {
    flags |= OldGenerationPageFlags(
        heap()->incremental_marking()->marking_mode(), space);
    if (!IsAnyLargeSpace(space) &&
        heap()->incremental_marking()->black_allocation() &&
        v8_flags.black_allocated_pages) {
      // Disable the write barrier for objects pointing to this page. We don't
      // need to trigger the barrier for pointers to old black-allocated pages,
      // since those are never considered for evacuation. However, we have to
      // keep the old->shared remembered set across multiple GCs, so those
      // pointers still need to be recorded.
      if (!IsAnyWritableSharedSpace(space)) {
        flags &= ~MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING;
      }
      // And mark the page as black allocated.
      flags |= MemoryChunk::BLACK_ALLOCATED;
    }
  }

  // All pages of a shared heap need to be marked with this flag.
  if (IsAnyWritableSharedSpace(space)) {
    flags |= MemoryChunk::IN_WRITABLE_SHARED_SPACE;
  }

  return flags;
}

void MutablePageMetadata::SetOldGenerationPageFlags(MarkingMode marking_mode) {
  const auto owner = owner_identity();
  MemoryChunk::MainThreadFlags flags_to_set =
      OldGenerationPageFlags(marking_mode, owner);
  MemoryChunk::MainThreadFlags flags_to_clear = MemoryChunk::NO_FLAGS;

  if (marking_mode != MarkingMode::kMajorMarking) {
    if (IsAnyWritableSharedSpace(owner)) {
      // No need to track OLD_TO_NEW or OLD_TO_SHARED within the shared space.
      flags_to_clear |= MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING |
                        MemoryChunk::INCREMENTAL_MARKING;
    } else {
      flags_to_clear |= MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING;
      if (marking_mode != MarkingMode::kMinorMarking) {
        flags_to_clear |= MemoryChunk::INCREMENTAL_MARKING;
      }
    }
  }

  SetFlagsNonExecutable(flags_to_set, flags_to_set);
  ClearFlagsNonExecutable(flags_to_clear);
}

void MutablePageMetadata::SetYoungGenerationPageFlags(
    MarkingMode marking_mode) {
  const MemoryChunk::MainThreadFlags flags_to_set =
      YoungGenerationPageFlags(marking_mode);
  MemoryChunk::MainThreadFlags flags_to_clear = MemoryChunk::NO_FLAGS;
  if (marking_mode == MarkingMode::kNoMarking) {
    flags_to_clear |= MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING;
    flags_to_clear |= MemoryChunk::INCREMENTAL_MARKING;
  }

  SetFlagsNonExecutable(flags_to_set, flags_to_set);
  ClearFlagsNonExecutable(flags_to_clear);
}

size_t MutablePageMetadata::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits() || is_large()) return size();
  return active_system_pages_->Size(MemoryAllocator::GetCommitPageSizeBits());
}

void MutablePageMetadata::ReleaseAllocatedMemoryNeededForWritableChunk() {
  DCHECK(SweepingDone());

  active_system_pages_.reset();

  possibly_empty_buckets_.Release();
  ReleaseSlotSet(OLD_TO_NEW);
  ReleaseSlotSet(OLD_TO_NEW_BACKGROUND);
  ReleaseSlotSet(OLD_TO_OLD);
  ReleaseSlotSet(TRUSTED_TO_CODE);
  ReleaseSlotSet(OLD_TO_SHARED);
  ReleaseSlotSet(TRUSTED_TO_TRUSTED);
  ReleaseSlotSet(TRUSTED_TO_SHARED_TRUSTED);
  ReleaseSlotSet(SURVIVOR_TO_EXTERNAL_POINTER);
  ReleaseTypedSlotSet(OLD_TO_NEW);
  ReleaseTypedSlotSet(OLD_TO_OLD);
  ReleaseTypedSlotSet(OLD_TO_SHARED);

  if (!is_large()) {
    PageMetadata* page = static_cast<PageMetadata*>(this);
    page->ReleaseFreeListCategories();
  }
}

void MutablePageMetadata::ReleaseAllAllocatedMemory() {
  ReleaseAllocatedMemoryNeededForWritableChunk();
}

SlotSet* MutablePageMetadata::AllocateSlotSet(RememberedSetType type) {
  SlotSet* new_slot_set = SlotSet::Allocate(BucketsInSlotSet());
  SlotSet* old_slot_set = base::AsAtomicPointer::AcquireRelease_CompareAndSwap(
      &slot_set_[type], nullptr, new_slot_set);
  if (old_slot_set) {
    SlotSet::Delete(new_slot_set);
    new_slot_set = old_slot_set;
  }
  DCHECK_NOT_NULL(new_slot_set);
  return new_slot_set;
}

void MutablePageMetadata::ReleaseSlotSet(RememberedSetType type) {
  SlotSet* slot_set = slot_set_[type];
  if (slot_set) {
    slot_set_[type] = nullptr;
    SlotSet::Delete(slot_set);
  }
}

TypedSlotSet* MutablePageMetadata::AllocateTypedSlotSet(
    RememberedSetType type) {
  TypedSlotSet* typed_slot_set = new TypedSlotSet(ChunkAddress());
  TypedSlotSet* old_value = base::AsAtomicPointer::Release_CompareAndSwap(
      &typed_slot_set_[type], nullptr, typed_slot_set);
  if (old_value) {
    delete typed_slot_set;
    typed_slot_set = old_value;
  }
  DCHECK(typed_slot_set);
  return typed_slot_set;
}

void MutablePageMetadata::ReleaseTypedSlotSet(RememberedSetType type) {
  TypedSlotSet* typed_slot_set = typed_slot_set_[type];
  if (typed_slot_set) {
    typed_slot_set_[type] = nullptr;
    delete typed_slot_set;
  }
}

bool MutablePageMetadata::ContainsAnySlots() const {
  for (int rs_type = 0; rs_type < NUMBER_OF_REMEMBERED_SET_TYPES; rs_type++) {
    if (slot_set_[rs_type] || typed_slot_set_[rs_type]) {
      return true;
    }
  }
  return false;
}

int MutablePageMetadata::ComputeFreeListsLength() {
  int length = 0;
  for (int cat = kFirstCategory; cat <= owner()->free_list()->last_category();
       cat++) {
    if (categories_[cat] != nullptr) {
      length += categories_[cat]->FreeListLength();
    }
  }
  return length;
}

bool MutablePageMetadata::IsLivenessClear() const {
  CHECK_IMPLIES(marking_bitmap()->IsClean(), live_bytes() == 0);
  return marking_bitmap()->IsClean();
}

void MutablePageMetadata::SetFlagMaybeExecutable(MemoryChunk::Flag flag) {
  if (is_executable()) {
    RwxMemoryWriteScope scope("Set a MemoryChunk flag in executable memory.");
    SetFlagUnlocked(flag);
  } else {
    SetFlagUnlocked(flag);
  }
}

void MutablePageMetadata::ClearFlagMaybeExecutable(MemoryChunk::Flag flag) {
  if (is_executable()) {
    RwxMemoryWriteScope scope("Set a MemoryChunk flag in executable memory.");
    ClearFlagUnlocked(flag);
  } else {
    ClearFlagUnlocked(flag);
  }
}

void MutablePageMetadata::MarkNeverEvacuate() { set_never_evacuate(); }

}  // namespace v8::internal
