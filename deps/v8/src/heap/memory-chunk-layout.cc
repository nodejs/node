// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk-layout.h"

#include "src/heap/marking.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk.h"

namespace v8 {
namespace internal {

size_t MemoryChunkLayout::CodePageGuardStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return ::RoundUp(MemoryChunk::kHeaderSize + Bitmap::kSize,
                   MemoryAllocator::GetCommitPageSize());
}

size_t MemoryChunkLayout::CodePageGuardSize() {
  return MemoryAllocator::GetCommitPageSize();
}

intptr_t MemoryChunkLayout::ObjectStartOffsetInCodePage() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return CodePageGuardStartOffset() + CodePageGuardSize();
}

intptr_t MemoryChunkLayout::ObjectEndOffsetInCodePage() {
  // We are guarding code pages: the last OS page will be protected as
  // non-writable.
  return MemoryChunk::kPageSize -
         static_cast<int>(MemoryAllocator::GetCommitPageSize());
}

size_t MemoryChunkLayout::AllocatableMemoryInCodePage() {
  size_t memory = ObjectEndOffsetInCodePage() - ObjectStartOffsetInCodePage();
  return memory;
}

intptr_t MemoryChunkLayout::ObjectStartOffsetInDataPage() {
  return RoundUp(MemoryChunk::kHeaderSize + Bitmap::kSize, kDoubleSize);
}

size_t MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(
    AllocationSpace space) {
  if (space == CODE_SPACE) {
    return ObjectStartOffsetInCodePage();
  }
  return ObjectStartOffsetInDataPage();
}

size_t MemoryChunkLayout::AllocatableMemoryInDataPage() {
  size_t memory = MemoryChunk::kPageSize - ObjectStartOffsetInDataPage();
  DCHECK_LE(kMaxRegularHeapObjectSize, memory);
  return memory;
}

size_t MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
    AllocationSpace space) {
  if (space == CODE_SPACE) {
    return AllocatableMemoryInCodePage();
  }
  return AllocatableMemoryInDataPage();
}

int MemoryChunkLayout::MaxRegularCodeObjectSize() {
  int size = static_cast<int>(AllocatableMemoryInCodePage() / 2);
  DCHECK_LE(size, kMaxRegularHeapObjectSize);
  return size;
}

}  // namespace internal
}  // namespace v8
