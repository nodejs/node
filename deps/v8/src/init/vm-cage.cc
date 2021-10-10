// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/vm-cage.h"

#include "include/v8-internal.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/lazy-instance.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

#ifdef V8_VIRTUAL_MEMORY_CAGE

bool V8VirtualMemoryCage::Initialize(PageAllocator* page_allocator) {
  constexpr bool use_guard_regions = true;
  return Initialize(page_allocator, kVirtualMemoryCageSize, use_guard_regions);
}

bool V8VirtualMemoryCage::Initialize(v8::PageAllocator* page_allocator,
                                     size_t size, bool use_guard_regions) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK_GE(size, kVirtualMemoryCageMinimumSize);

  size_t reservation_size = size;
  if (use_guard_regions) {
    reservation_size += 2 * kVirtualMemoryCageGuardRegionSize;
  }

  base_ = reinterpret_cast<Address>(page_allocator->AllocatePages(
      nullptr, reservation_size, kVirtualMemoryCageAlignment,
      PageAllocator::kNoAccess));
  if (!base_) return false;

  if (use_guard_regions) {
    base_ += kVirtualMemoryCageGuardRegionSize;
    has_guard_regions_ = true;
  }

  page_allocator_ = page_allocator;
  size_ = size;

  data_cage_page_allocator_ = std::make_unique<base::BoundedPageAllocator>(
      page_allocator_, data_cage_base(), data_cage_size(),
      page_allocator_->AllocatePageSize());

  initialized_ = true;

  return true;
}

void V8VirtualMemoryCage::TearDown() {
  if (initialized_) {
    data_cage_page_allocator_.reset();
    Address reservation_base = base_;
    size_t reservation_size = size_;
    if (has_guard_regions_) {
      reservation_base -= kVirtualMemoryCageGuardRegionSize;
      reservation_size += 2 * kVirtualMemoryCageGuardRegionSize;
    }
    CHECK(page_allocator_->FreePages(reinterpret_cast<void*>(reservation_base),
                                     reservation_size));
    page_allocator_ = nullptr;
    base_ = kNullAddress;
    size_ = 0;
    initialized_ = false;
    has_guard_regions_ = false;
  }
  disabled_ = false;
}

DEFINE_LAZY_LEAKY_OBJECT_GETTER(V8VirtualMemoryCage,
                                GetProcessWideVirtualMemoryCage)

#endif

}  // namespace internal
}  // namespace v8
