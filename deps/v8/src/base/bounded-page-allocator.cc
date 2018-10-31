// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bounded-page-allocator.h"

namespace v8 {
namespace base {

BoundedPageAllocator::BoundedPageAllocator(v8::PageAllocator* page_allocator,
                                           Address start, size_t size,
                                           size_t allocate_page_size)
    : allocate_page_size_(allocate_page_size),
      commit_page_size_(page_allocator->CommitPageSize()),
      page_allocator_(page_allocator),
      region_allocator_(start, size, allocate_page_size_) {
  CHECK_NOT_NULL(page_allocator);
  CHECK(IsAligned(allocate_page_size, page_allocator->AllocatePageSize()));
  CHECK(IsAligned(allocate_page_size_, commit_page_size_));
}

BoundedPageAllocator::Address BoundedPageAllocator::begin() const {
  return region_allocator_.begin();
}

size_t BoundedPageAllocator::size() const { return region_allocator_.size(); }

void* BoundedPageAllocator::AllocatePages(void* hint, size_t size,
                                          size_t alignment,
                                          PageAllocator::Permission access) {
  MutexGuard guard(&mutex_);
  CHECK(IsAligned(alignment, region_allocator_.page_size()));

  // Region allocator does not support alignments bigger than it's own
  // allocation alignment.
  CHECK_LE(alignment, allocate_page_size_);

  // TODO(ishell): Consider using randomized version here.
  Address address = region_allocator_.AllocateRegion(size);
  if (address == RegionAllocator::kAllocationFailure) {
    return nullptr;
  }
  CHECK(page_allocator_->SetPermissions(reinterpret_cast<void*>(address), size,
                                        access));
  return reinterpret_cast<void*>(address);
}

bool BoundedPageAllocator::AllocatePagesAt(Address address, size_t size,
                                           PageAllocator::Permission access) {
  CHECK(IsAligned(address, allocate_page_size_));
  CHECK(IsAligned(size, allocate_page_size_));
  CHECK(region_allocator_.contains(address, size));

  if (!region_allocator_.AllocateRegionAt(address, size)) {
    return false;
  }
  CHECK(page_allocator_->SetPermissions(reinterpret_cast<void*>(address), size,
                                        access));
  return true;
}

bool BoundedPageAllocator::FreePages(void* raw_address, size_t size) {
  MutexGuard guard(&mutex_);

  Address address = reinterpret_cast<Address>(raw_address);
  size_t freed_size = region_allocator_.FreeRegion(address);
  if (freed_size != size) return false;
  CHECK(page_allocator_->SetPermissions(raw_address, size,
                                        PageAllocator::kNoAccess));
  return true;
}

bool BoundedPageAllocator::ReleasePages(void* raw_address, size_t size,
                                        size_t new_size) {
  Address address = reinterpret_cast<Address>(raw_address);
  CHECK(IsAligned(address, allocate_page_size_));

  DCHECK_LT(new_size, size);
  DCHECK(IsAligned(size - new_size, commit_page_size_));

  // Check if we freed any allocatable pages by this release.
  size_t allocated_size = RoundUp(size, allocate_page_size_);
  size_t new_allocated_size = RoundUp(new_size, allocate_page_size_);

#ifdef DEBUG
  {
    // There must be an allocated region at given |address| of a size not
    // smaller than |size|.
    MutexGuard guard(&mutex_);
    CHECK_EQ(allocated_size, region_allocator_.CheckRegion(address));
  }
#endif

  if (new_allocated_size < allocated_size) {
    MutexGuard guard(&mutex_);
    region_allocator_.TrimRegion(address, new_allocated_size);
  }

  // Keep the region in "used" state just uncommit some pages.
  Address free_address = address + new_size;
  size_t free_size = size - new_size;
  return page_allocator_->SetPermissions(reinterpret_cast<void*>(free_address),
                                         free_size, PageAllocator::kNoAccess);
}

bool BoundedPageAllocator::SetPermissions(void* address, size_t size,
                                          PageAllocator::Permission access) {
  DCHECK(IsAligned(reinterpret_cast<Address>(address), commit_page_size_));
  DCHECK(IsAligned(size, commit_page_size_));
  DCHECK(region_allocator_.contains(reinterpret_cast<Address>(address), size));
  return page_allocator_->SetPermissions(address, size, access);
}

bool BoundedPageAllocator::DiscardSystemPages(void* address, size_t size) {
  return page_allocator_->DiscardSystemPages(address, size);
}

}  // namespace base
}  // namespace v8
