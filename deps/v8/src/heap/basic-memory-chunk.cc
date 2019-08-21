// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/basic-memory-chunk.h"

#include <cstdlib>

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/slots-inl.h"

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
  const Address base = reinterpret_cast<Address>(this);
  size_ = size;
  marking_bitmap_ = static_cast<Bitmap*>(calloc(1, Bitmap::kSize));
  header_sentinel_ = HeapObject::FromAddress(base).ptr();
  DCHECK(HasHeaderSentinel(area_start));
  area_start_ = area_start;
  area_end_ = area_end;
}

// static
bool BasicMemoryChunk::HasHeaderSentinel(Address slot_addr) {
  Address base = BaseAddress(slot_addr);
  if (slot_addr < base + kHeaderSize) return false;
  return HeapObject::FromAddress(base) ==
         ObjectSlot(base + kHeaderSentinelOffset).Relaxed_Load();
}

void BasicMemoryChunk::ReleaseMarkingBitmap() {
  DCHECK_NOT_NULL(marking_bitmap_);
  free(marking_bitmap_);
  marking_bitmap_ = nullptr;
}

}  // namespace internal
}  // namespace v8
