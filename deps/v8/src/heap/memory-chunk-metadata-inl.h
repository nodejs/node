// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_METADATA_INL_H_
#define V8_HEAP_MEMORY_CHUNK_METADATA_INL_H_

#include "src/heap/memory-chunk-metadata.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/memory-chunk-inl.h"

namespace v8 {
namespace internal {

// static
MemoryChunkMetadata* MemoryChunkMetadata::FromAddress(Address a) {
  return MemoryChunk::FromAddress(a)->Metadata();
}

// static
MemoryChunkMetadata* MemoryChunkMetadata::FromAddress(const Isolate* i,
                                                      Address a) {
  return MemoryChunk::FromAddress(a)->Metadata(i);
}

// static
MemoryChunkMetadata* MemoryChunkMetadata::FromHeapObject(Tagged<HeapObject> o) {
  return FromAddress(o.ptr());
}

// static
MemoryChunkMetadata* MemoryChunkMetadata::FromHeapObject(const Isolate* i,
                                                         Tagged<HeapObject> o) {
  return FromAddress(i, o.ptr());
}

// static
MemoryChunkMetadata* MemoryChunkMetadata::FromHeapObject(
    const HeapObjectLayout* o) {
  return FromAddress(reinterpret_cast<Address>(o));
}

// static
void MemoryChunkMetadata::UpdateHighWaterMark(Address mark) {
  if (mark == kNullAddress) {
    return;
  }
  // Need to subtract one from the mark because when a chunk is full the
  // top points to the next address after the chunk, which effectively belongs
  // to another chunk. See the comment to
  // PageMetadata::FromAllocationAreaAddress.
  MemoryChunkMetadata* chunk = MemoryChunkMetadata::FromAddress(mark - 1);
  intptr_t new_mark = static_cast<intptr_t>(mark - chunk->ChunkAddress());
  intptr_t old_mark = chunk->high_water_mark_.load(std::memory_order_relaxed);
  while ((new_mark > old_mark) &&
         !chunk->high_water_mark_.compare_exchange_weak(
             old_mark, new_mark, std::memory_order_acq_rel)) {
  }
}

AllocationSpace MemoryChunkMetadata::owner_identity() const {
  {
    AllowSandboxAccess temporary_sandbox_access;
    DCHECK_EQ(owner() == nullptr, Chunk()->InReadOnlySpace());
  }
  if (!owner()) {
    return RO_SPACE;
  }
  return owner()->identity();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_CHUNK_METADATA_INL_H_
