// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_INL_H_
#define V8_HEAP_CPPGC_HEAP_INL_H_

#include "src/heap/cppgc/heap.h"

#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"

namespace cppgc {
namespace internal {

void* Heap::Allocate(size_t size, GCInfoIndex index) {
  DCHECK(is_allocation_allowed());
  // TODO(chromium:1056170): This is merely a dummy implementation and will be
  // replaced with proper allocation code throughout the migration.
  size_t allocation_size = size + sizeof(HeapObjectHeader);
  // The allocation size calculation can overflow for large sizes.
  CHECK_GT(allocation_size, size);
  // calloc() provides stricter alignment guarantees than the GC. Allocate
  // a multiple of kAllocationGranularity to follow restrictions of
  // HeapObjectHeader.
  allocation_size = (allocation_size + kAllocationMask) & ~kAllocationMask;
  void* memory = allocator_->Allocate(allocation_size);
  HeapObjectHeader* header =
      new (memory) HeapObjectHeader(allocation_size, index);
  objects_.push_back(header);
  return header->Payload();
}

void* Heap::BasicAllocator::Allocate(size_t size) {
  // Can only allocate normal-sized objects.
  CHECK_GT(kLargeObjectSizeThreshold, size);
  if (current_ == nullptr || (current_ + size) > limit_) {
    GetNewPage();
  }
  void* memory = current_;
  current_ += size;
  return memory;
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_INL_H_
