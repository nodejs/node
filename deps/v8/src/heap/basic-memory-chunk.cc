// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/basic-memory-chunk.h"

#include <cstdlib>

#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

// Verify write barrier offsets match the the real offsets.
STATIC_ASSERT(BasicMemoryChunk::Flag::INCREMENTAL_MARKING ==
              heap_internals::MemoryChunk::kMarkingBit);
STATIC_ASSERT(BasicMemoryChunk::Flag::FROM_PAGE ==
              heap_internals::MemoryChunk::kFromPageBit);
STATIC_ASSERT(BasicMemoryChunk::Flag::TO_PAGE ==
              heap_internals::MemoryChunk::kToPageBit);
STATIC_ASSERT(BasicMemoryChunk::kFlagsOffset ==
              heap_internals::MemoryChunk::kFlagsOffset);
STATIC_ASSERT(BasicMemoryChunk::kHeapOffset ==
              heap_internals::MemoryChunk::kHeapOffset);

BasicMemoryChunk::BasicMemoryChunk(size_t size, Address area_start,
                                   Address area_end) {
  size_ = size;
  marking_bitmap_ = static_cast<Bitmap*>(calloc(1, Bitmap::kSize));
  area_start_ = area_start;
  area_end_ = area_end;
}

void BasicMemoryChunk::ReleaseMarkingBitmap() {
  DCHECK_NOT_NULL(marking_bitmap_);
  free(marking_bitmap_);
  marking_bitmap_ = nullptr;
}

// static
BasicMemoryChunk* BasicMemoryChunk::Initialize(Heap* heap, Address base,
                                               size_t size, Address area_start,
                                               Address area_end,
                                               BaseSpace* owner,
                                               VirtualMemory reservation) {
  BasicMemoryChunk* chunk = FromAddress(base);
  DCHECK_EQ(base, chunk->address());
  new (chunk) BasicMemoryChunk(size, area_start, area_end);

  chunk->heap_ = heap;
  chunk->set_owner(owner);
  chunk->reservation_ = std::move(reservation);
  chunk->high_water_mark_ = static_cast<intptr_t>(area_start - base);
  chunk->allocated_bytes_ = chunk->area_size();
  chunk->wasted_memory_ = 0;

  return chunk;
}

bool BasicMemoryChunk::InOldSpace() const {
  return owner()->identity() == OLD_SPACE;
}

bool BasicMemoryChunk::InLargeObjectSpace() const {
  return owner()->identity() == LO_SPACE;
}

#ifdef THREAD_SANITIZER
void BasicMemoryChunk::SynchronizedHeapLoad() {
  CHECK(reinterpret_cast<Heap*>(base::Acquire_Load(
            reinterpret_cast<base::AtomicWord*>(&heap_))) != nullptr ||
        InReadOnlySpace());
}
#endif

}  // namespace internal
}  // namespace v8
