// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/basic-memory-chunk.h"

#include <cstdlib>

#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-inl.h"
#include "src/objects/heap-object.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

// Verify write barrier offsets match the the real offsets.
static_assert(BasicMemoryChunk::Flag::IS_EXECUTABLE ==
              heap_internals::MemoryChunk::kIsExecutableBit);
static_assert(BasicMemoryChunk::Flag::IN_WRITABLE_SHARED_SPACE ==
              heap_internals::MemoryChunk::kInWritableSharedSpaceBit);
static_assert(BasicMemoryChunk::Flag::INCREMENTAL_MARKING ==
              heap_internals::MemoryChunk::kMarkingBit);
static_assert(BasicMemoryChunk::Flag::FROM_PAGE ==
              heap_internals::MemoryChunk::kFromPageBit);
static_assert(BasicMemoryChunk::Flag::TO_PAGE ==
              heap_internals::MemoryChunk::kToPageBit);
static_assert(BasicMemoryChunk::Flag::READ_ONLY_HEAP ==
              heap_internals::MemoryChunk::kReadOnlySpaceBit);
static_assert(BasicMemoryChunk::kFlagsOffset ==
              heap_internals::MemoryChunk::kFlagsOffset);
static_assert(BasicMemoryChunk::kHeapOffset ==
              heap_internals::MemoryChunk::kHeapOffset);

// static
constexpr BasicMemoryChunk::MainThreadFlags BasicMemoryChunk::kAllFlagsMask;
// static
constexpr BasicMemoryChunk::MainThreadFlags
    BasicMemoryChunk::kPointersToHereAreInterestingMask;
// static
constexpr BasicMemoryChunk::MainThreadFlags
    BasicMemoryChunk::kPointersFromHereAreInterestingMask;
// static
constexpr BasicMemoryChunk::MainThreadFlags
    BasicMemoryChunk::kEvacuationCandidateMask;
// static
constexpr BasicMemoryChunk::MainThreadFlags
    BasicMemoryChunk::kIsInYoungGenerationMask;
// static
constexpr BasicMemoryChunk::MainThreadFlags BasicMemoryChunk::kIsLargePageMask;
// static
constexpr BasicMemoryChunk::MainThreadFlags
    BasicMemoryChunk::kSkipEvacuationSlotsRecordingMask;

BasicMemoryChunk::BasicMemoryChunk(Heap* heap, BaseSpace* space,
                                   size_t chunk_size, Address area_start,
                                   Address area_end, VirtualMemory reservation)
    : size_(chunk_size),
      heap_(heap),
      area_start_(area_start),
      area_end_(area_end),
      allocated_bytes_(area_end - area_start),
      high_water_mark_(area_start - reinterpret_cast<Address>(this)),
      owner_(space),
      reservation_(std::move(reservation)) {}

bool BasicMemoryChunk::InOldSpace() const {
  return owner()->identity() == OLD_SPACE;
}

bool BasicMemoryChunk::InLargeObjectSpace() const {
  return owner()->identity() == LO_SPACE;
}

#ifdef THREAD_SANITIZER
void BasicMemoryChunk::SynchronizedHeapLoad() const {
  CHECK(reinterpret_cast<Heap*>(
            base::Acquire_Load(reinterpret_cast<base::AtomicWord*>(
                &(const_cast<BasicMemoryChunk*>(this)->heap_)))) != nullptr ||
        IsFlagSet(READ_ONLY_HEAP));
}
#endif

class BasicMemoryChunkValidator {
  // Computed offsets should match the compiler generated ones.
  static_assert(BasicMemoryChunk::kSizeOffset ==
                offsetof(BasicMemoryChunk, size_));
  static_assert(BasicMemoryChunk::kFlagsOffset ==
                offsetof(BasicMemoryChunk, main_thread_flags_));
  static_assert(BasicMemoryChunk::kHeapOffset ==
                offsetof(BasicMemoryChunk, heap_));
  static_assert(offsetof(BasicMemoryChunk, size_) ==
                MemoryChunkLayout::kSizeOffset);
  static_assert(offsetof(BasicMemoryChunk, heap_) ==
                MemoryChunkLayout::kHeapOffset);
  static_assert(offsetof(BasicMemoryChunk, area_start_) ==
                MemoryChunkLayout::kAreaStartOffset);
  static_assert(offsetof(BasicMemoryChunk, area_end_) ==
                MemoryChunkLayout::kAreaEndOffset);
  static_assert(offsetof(BasicMemoryChunk, allocated_bytes_) ==
                MemoryChunkLayout::kAllocatedBytesOffset);
  static_assert(offsetof(BasicMemoryChunk, wasted_memory_) ==
                MemoryChunkLayout::kWastedMemoryOffset);
  static_assert(offsetof(BasicMemoryChunk, high_water_mark_) ==
                MemoryChunkLayout::kHighWaterMarkOffset);
  static_assert(offsetof(BasicMemoryChunk, owner_) ==
                MemoryChunkLayout::kOwnerOffset);
  static_assert(offsetof(BasicMemoryChunk, reservation_) ==
                MemoryChunkLayout::kReservationOffset);
};

}  // namespace internal
}  // namespace v8
