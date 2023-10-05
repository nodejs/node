// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_INL_H_
#define V8_HEAP_MEMORY_CHUNK_INL_H_

#include "src/heap/memory-chunk.h"
#include "src/heap/spaces-inl.h"

namespace v8 {
namespace internal {

void MemoryChunk::IncrementExternalBackingStoreBytes(
    ExternalBackingStoreType type, size_t amount) {
#ifndef V8_ENABLE_THIRD_PARTY_HEAP
  base::CheckedIncrement(&external_backing_store_bytes_[static_cast<int>(type)],
                         amount);
  owner()->IncrementExternalBackingStoreBytes(type, amount);
#endif
}

void MemoryChunk::DecrementExternalBackingStoreBytes(
    ExternalBackingStoreType type, size_t amount) {
#ifndef V8_ENABLE_THIRD_PARTY_HEAP
  base::CheckedDecrement(&external_backing_store_bytes_[static_cast<int>(type)],
                         amount);
  owner()->DecrementExternalBackingStoreBytes(type, amount);
#endif
}

void MemoryChunk::MoveExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                MemoryChunk* from,
                                                MemoryChunk* to,
                                                size_t amount) {
  DCHECK_NOT_NULL(from->owner());
  DCHECK_NOT_NULL(to->owner());
  base::CheckedDecrement(
      &(from->external_backing_store_bytes_[static_cast<int>(type)]), amount);
  base::CheckedIncrement(
      &(to->external_backing_store_bytes_[static_cast<int>(type)]), amount);
  Space::MoveExternalBackingStoreBytes(type, from->owner(), to->owner(),
                                       amount);
}

AllocationSpace MemoryChunk::owner_identity() const {
  if (InReadOnlySpace()) return RO_SPACE;
  return owner()->identity();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_CHUNK_INL_H_
