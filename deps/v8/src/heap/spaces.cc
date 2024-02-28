// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"

#include <algorithm>
#include <cinttypes>
#include <utility>

#include "src/base/bits.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/macros.h"
#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/heap/base/active-system-pages.h"
#include "src/heap/concurrent-allocator.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/large-spaces.h"
#include "src/heap/main-allocator-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/remembered-set.h"
#include "src/heap/slot-set.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

SpaceWithLinearArea::SpaceWithLinearArea(
    Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list,
    CompactionSpaceKind compaction_space_kind,
    MainAllocator::SupportsExtendingLAB supports_extending_lab,
    LinearAllocationArea& allocation_info)
    : Space(heap, id, std::move(free_list)) {
  owned_allocator_.emplace(heap, this, compaction_space_kind,
                           supports_extending_lab, allocation_info);
  allocator_ = &owned_allocator_.value();
}

SpaceWithLinearArea::SpaceWithLinearArea(
    Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list,
    CompactionSpaceKind compaction_space_kind,
    MainAllocator::SupportsExtendingLAB supports_extending_lab)
    : Space(heap, id, std::move(free_list)) {
  owned_allocator_.emplace(heap, this, compaction_space_kind,
                           supports_extending_lab);
  allocator_ = &owned_allocator_.value();
}

SpaceWithLinearArea::SpaceWithLinearArea(
    Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list,
    CompactionSpaceKind compaction_space_kind, MainAllocator* allocator)
    : Space(heap, id, std::move(free_list)), allocator_(allocator) {}

LinearAllocationArea LocalAllocationBuffer::CloseAndMakeIterable() {
  if (IsValid()) {
    MakeIterable();
    const LinearAllocationArea old_info = allocation_info_;
    allocation_info_ = LinearAllocationArea(kNullAddress, kNullAddress);
    return old_info;
  }
  return LinearAllocationArea(kNullAddress, kNullAddress);
}

void LocalAllocationBuffer::MakeIterable() {
  if (IsValid()) {
    heap_->CreateFillerObjectAtBackground(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()));
  }
}

LocalAllocationBuffer::LocalAllocationBuffer(
    Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT
    : heap_(heap),
      allocation_info_(allocation_info) {}

LocalAllocationBuffer::LocalAllocationBuffer(LocalAllocationBuffer&& other)
    V8_NOEXCEPT {
  *this = std::move(other);
}

LocalAllocationBuffer& LocalAllocationBuffer::operator=(
    LocalAllocationBuffer&& other) V8_NOEXCEPT {
  heap_ = other.heap_;
  allocation_info_ = other.allocation_info_;

  other.allocation_info_.Reset(kNullAddress, kNullAddress);
  return *this;
}

SpaceIterator::SpaceIterator(Heap* heap)
    : heap_(heap), current_space_(FIRST_MUTABLE_SPACE) {}

SpaceIterator::~SpaceIterator() = default;

bool SpaceIterator::HasNext() {
  while (current_space_ <= LAST_MUTABLE_SPACE) {
    Space* space = heap_->space(current_space_);
    if (space) return true;
    ++current_space_;
  }

  // No more spaces left.
  return false;
}

Space* SpaceIterator::Next() {
  DCHECK_LE(current_space_, LAST_MUTABLE_SPACE);
  Space* space = heap_->space(current_space_++);
  DCHECK_NOT_NULL(space);
  return space;
}

}  // namespace internal
}  // namespace v8
