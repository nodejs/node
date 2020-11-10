// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/object-allocator.h"

#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/object-allocator-inl.h"
#include "src/heap/cppgc/object-start-bitmap.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/sweeper.h"

namespace cppgc {
namespace internal {
namespace {

void* AllocateLargeObject(RawHeap* raw_heap, LargePageSpace* space, size_t size,
                          GCInfoIndex gcinfo) {
  LargePage* page = LargePage::Create(space, size);
  auto* header = new (page->ObjectHeader())
      HeapObjectHeader(HeapObjectHeader::kLargeObjectSizeInHeader, gcinfo);

  return header->Payload();
}

}  // namespace

ObjectAllocator::ObjectAllocator(RawHeap* heap) : raw_heap_(heap) {}

void* ObjectAllocator::OutOfLineAllocate(NormalPageSpace* space, size_t size,
                                         GCInfoIndex gcinfo) {
  DCHECK_EQ(0, size & kAllocationMask);
  DCHECK_LE(kFreeListEntrySize, size);

  // 1. If this allocation is big enough, allocate a large object.
  if (size >= kLargeObjectSizeThreshold) {
    auto* large_space = LargePageSpace::From(
        raw_heap_->Space(RawHeap::RegularSpaceType::kLarge));
    return AllocateLargeObject(raw_heap_, large_space, size, gcinfo);
  }

  // 2. Try to allocate from the freelist.
  if (void* result = AllocateFromFreeList(space, size, gcinfo)) {
    return result;
  }

  // 3. Lazily sweep pages of this heap until we find a freed area for
  // this allocation or we finish sweeping all pages of this heap.
  // TODO(chromium:1056170): Add lazy sweep.

  // 4. Complete sweeping.
  raw_heap_->heap()->sweeper().Finish();

  // 5. Add a new page to this heap.
  NormalPage::Create(space);

  // 6. Try to allocate from the freelist. This allocation must succeed.
  void* result = AllocateFromFreeList(space, size, gcinfo);
  CPPGC_CHECK(result);

  return result;
}

void* ObjectAllocator::AllocateFromFreeList(NormalPageSpace* space, size_t size,
                                            GCInfoIndex gcinfo) {
  const FreeList::Block entry = space->free_list().Allocate(size);
  if (!entry.address) return nullptr;

  auto& current_lab = space->linear_allocation_buffer();
  if (current_lab.size()) {
    space->AddToFreeList(current_lab.start(), current_lab.size());
  }

  current_lab.Set(static_cast<Address>(entry.address), entry.size);
  NormalPage::From(BasePage::FromPayload(current_lab.start()))
      ->object_start_bitmap()
      .ClearBit(current_lab.start());
  return AllocateObjectOnSpace(space, size, gcinfo);
}

}  // namespace internal
}  // namespace cppgc
