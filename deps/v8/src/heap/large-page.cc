// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/large-page.h"

#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/remembered-set.h"

namespace v8 {
namespace internal {

class Heap;

// This check is here to ensure that the lower 32 bits of any real heap object
// can't overlap with the lower 32 bits of cleared weak reference value and
// therefore it's enough to compare only the lower 32 bits of a MaybeObject in
// order to figure out if it's a cleared weak reference or not.
static_assert(kClearedWeakHeapObjectLower32 < LargePage::kHeaderSize);

LargePage::LargePage(Heap* heap, BaseSpace* space, size_t chunk_size,
                     Address area_start, Address area_end,
                     VirtualMemory reservation, Executability executable)
    : MemoryChunk(heap, space, chunk_size, area_start, area_end,
                  std::move(reservation), executable, PageSize::kLarge) {
  static_assert(LargePage::kMaxCodePageSize <= TypedSlotSet::kMaxOffset);

  if (executable && chunk_size > LargePage::kMaxCodePageSize) {
    FATAL("Code page is too large.");
  }

  SetFlag(MemoryChunk::LARGE_PAGE);
  list_node().Initialize();
}

LargePage* LargePage::Initialize(Heap* heap, MemoryChunk* chunk,
                                 Executability executable) {
  if (executable && chunk->size() > LargePage::kMaxCodePageSize) {
    static_assert(LargePage::kMaxCodePageSize <= TypedSlotSet::kMaxOffset);
    FATAL("Code page is too large.");
  }

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(chunk->area_start(), chunk->area_size());

  chunk->SetFlag(MemoryChunk::LARGE_PAGE);
  chunk->list_node().Initialize();
  return LargePage::cast(chunk);
}

void LargePage::ClearOutOfLiveRangeSlots(Address free_start) {
  DCHECK_NULL(slot_set<OLD_TO_NEW>());
  DCHECK_NULL(typed_slot_set<OLD_TO_NEW>());

  DCHECK_NULL(slot_set<OLD_TO_NEW_BACKGROUND>());
  DCHECK_NULL(typed_slot_set<OLD_TO_NEW_BACKGROUND>());

  DCHECK_NULL(slot_set<OLD_TO_OLD>());
  DCHECK_NULL(typed_slot_set<OLD_TO_OLD>());

  // area_end() might not be aligned to a full bucket size with large objects.
  // Align it to bucket size such that the following RemoveRange invocation just
  // drops the whole bucket and the bucket is reset to nullptr.
  Address aligned_area_end = address() + SlotSet::OffsetForBucket(buckets());
  DCHECK_LE(area_end(), aligned_area_end);
  RememberedSet<OLD_TO_SHARED>::RemoveRange(this, free_start, aligned_area_end,
                                            SlotSet::FREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_SHARED>::RemoveRangeTyped(this, free_start, area_end());
}

}  // namespace internal
}  // namespace v8
