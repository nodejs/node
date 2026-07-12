// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_PAGE_INL_H_
#define V8_HEAP_BASE_PAGE_INL_H_

#include "src/heap/base-page.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/memory-chunk-inl.h"

namespace v8::internal {

// static
BasePage* BasePage::FromAddress(Address a) {
  return MemoryChunk::FromAddress(a)->Metadata();
}

// static
BasePage* BasePage::FromAddress(const Isolate* i, Address a) {
  return MemoryChunk::FromAddress(a)->Metadata(i);
}

// static
BasePage* BasePage::FromHeapObject(Tagged<HeapObject> o) {
  return FromAddress(o.ptr());
}

// static
BasePage* BasePage::FromHeapObject(const Isolate* i, Tagged<HeapObject> o) {
  return FromAddress(i, o.ptr());
}

// static
void BasePage::UpdateHighWaterMark(Address mark) {
  if (mark == kNullAddress) {
    return;
  }
  // Need to subtract one from the mark because when a chunk is full the
  // top points to the next address after the chunk, which effectively belongs
  // to another chunk. See the comment to
  // NormalPage::FromAllocationAreaAddress.
  BasePage* chunk = BasePage::FromAddress(Isolate::Current(), mark - 1);
  intptr_t new_mark = static_cast<intptr_t>(mark - chunk->ChunkAddress());
  intptr_t old_mark = chunk->high_water_mark_.load(std::memory_order_relaxed);
  while ((new_mark > old_mark) &&
         !chunk->high_water_mark_.compare_exchange_weak(
             old_mark, new_mark, std::memory_order_acq_rel)) {
  }
}

bool BasePage::IsWritable() const {
  const bool is_sealed_ro = IsSealedReadOnlySpaceField::decode(flags_);
#ifdef DEBUG
  DCHECK_IMPLIES(is_sealed_ro, Chunk()->InReadOnlySpace());
  DCHECK_IMPLIES(is_sealed_ro, heap_ == nullptr);
  DCHECK_IMPLIES(is_sealed_ro, owner_ == nullptr);
#endif  // DEBUG
  return !is_sealed_ro;
}

AllocationSpace BasePage::owner_identity() const {
  if (!owner()) {
    return RO_SPACE;
  }
  return owner()->identity();
}

}  // namespace v8::internal

#endif  // V8_HEAP_BASE_PAGE_INL_H_
