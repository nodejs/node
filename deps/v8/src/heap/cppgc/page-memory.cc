// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/page-memory.h"

#include "src/base/macros.h"
#include "src/base/sanitizer/asan.h"

namespace cppgc {
namespace internal {

namespace {

void Unprotect(PageAllocator* allocator, const PageMemory& page_memory) {
  if (SupportsCommittingGuardPages(allocator)) {
    CHECK(allocator->SetPermissions(page_memory.writeable_region().base(),
                                    page_memory.writeable_region().size(),
                                    PageAllocator::Permission::kReadWrite));
  } else {
    // No protection in case the allocator cannot commit at the required
    // granularity. Only protect if the allocator supports committing at that
    // granularity.
    //
    // The allocator needs to support committing the overall range.
    CHECK_EQ(0u,
             page_memory.overall_region().size() % allocator->CommitPageSize());
    CHECK(allocator->SetPermissions(page_memory.overall_region().base(),
                                    page_memory.overall_region().size(),
                                    PageAllocator::Permission::kReadWrite));
  }
}

void Protect(PageAllocator* allocator, const PageMemory& page_memory) {
  if (SupportsCommittingGuardPages(allocator)) {
    // Swap the same region, providing the OS with a chance for fast lookup and
    // change.
    CHECK(allocator->SetPermissions(page_memory.writeable_region().base(),
                                    page_memory.writeable_region().size(),
                                    PageAllocator::Permission::kNoAccess));
  } else {
    // See Unprotect().
    CHECK_EQ(0u,
             page_memory.overall_region().size() % allocator->CommitPageSize());
    CHECK(allocator->SetPermissions(page_memory.overall_region().base(),
                                    page_memory.overall_region().size(),
                                    PageAllocator::Permission::kNoAccess));
  }
}

MemoryRegion ReserveMemoryRegion(PageAllocator* allocator,
                                 size_t allocation_size) {
  void* region_memory =
      allocator->AllocatePages(nullptr, allocation_size, kPageSize,
                               PageAllocator::Permission::kNoAccess);
  const MemoryRegion reserved_region(static_cast<Address>(region_memory),
                                     allocation_size);
  DCHECK_EQ(reserved_region.base() + allocation_size, reserved_region.end());
  return reserved_region;
}

void FreeMemoryRegion(PageAllocator* allocator,
                      const MemoryRegion& reserved_region) {
  // Make sure pages returned to OS are unpoisoned.
  ASAN_UNPOISON_MEMORY_REGION(reserved_region.base(), reserved_region.size());
  allocator->FreePages(reserved_region.base(), reserved_region.size());
}

}  // namespace

PageMemoryRegion::PageMemoryRegion(PageAllocator* allocator,
                                   MemoryRegion reserved_region, bool is_large)
    : allocator_(allocator),
      reserved_region_(reserved_region),
      is_large_(is_large) {}

PageMemoryRegion::~PageMemoryRegion() {
  FreeMemoryRegion(allocator_, reserved_region());
}

// static
constexpr size_t NormalPageMemoryRegion::kNumPageRegions;

NormalPageMemoryRegion::NormalPageMemoryRegion(PageAllocator* allocator)
    : PageMemoryRegion(allocator,
                       ReserveMemoryRegion(
                           allocator, RoundUp(kPageSize * kNumPageRegions,
                                              allocator->AllocatePageSize())),
                       false) {
#ifdef DEBUG
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    DCHECK_EQ(false, page_memories_in_use_[i]);
  }
#endif  // DEBUG
}

NormalPageMemoryRegion::~NormalPageMemoryRegion() = default;

void NormalPageMemoryRegion::Allocate(Address writeable_base) {
  const size_t index = GetIndex(writeable_base);
  ChangeUsed(index, true);
  Unprotect(allocator_, GetPageMemory(index));
}

void NormalPageMemoryRegion::Free(Address writeable_base) {
  const size_t index = GetIndex(writeable_base);
  ChangeUsed(index, false);
  Protect(allocator_, GetPageMemory(index));
}

void NormalPageMemoryRegion::UnprotectForTesting() {
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    Unprotect(allocator_, GetPageMemory(i));
  }
}

LargePageMemoryRegion::LargePageMemoryRegion(PageAllocator* allocator,
                                             size_t length)
    : PageMemoryRegion(allocator,
                       ReserveMemoryRegion(
                           allocator, RoundUp(length + 2 * kGuardPageSize,
                                              allocator->AllocatePageSize())),
                       true) {}

LargePageMemoryRegion::~LargePageMemoryRegion() = default;

void LargePageMemoryRegion::UnprotectForTesting() {
  Unprotect(allocator_, GetPageMemory());
}

PageMemoryRegionTree::PageMemoryRegionTree() = default;

PageMemoryRegionTree::~PageMemoryRegionTree() = default;

void PageMemoryRegionTree::Add(PageMemoryRegion* region) {
  DCHECK(region);
  auto result = set_.emplace(region->reserved_region().base(), region);
  USE(result);
  DCHECK(result.second);
}

void PageMemoryRegionTree::Remove(PageMemoryRegion* region) {
  DCHECK(region);
  auto size = set_.erase(region->reserved_region().base());
  USE(size);
  DCHECK_EQ(1u, size);
}

NormalPageMemoryPool::NormalPageMemoryPool() = default;

NormalPageMemoryPool::~NormalPageMemoryPool() = default;

void NormalPageMemoryPool::Add(size_t bucket, NormalPageMemoryRegion* pmr,
                               Address writeable_base) {
  DCHECK_LT(bucket, kNumPoolBuckets);
  pool_[bucket].push_back(std::make_pair(pmr, writeable_base));
}

std::pair<NormalPageMemoryRegion*, Address> NormalPageMemoryPool::Take(
    size_t bucket) {
  DCHECK_LT(bucket, kNumPoolBuckets);
  if (pool_[bucket].empty()) return {nullptr, nullptr};
  std::pair<NormalPageMemoryRegion*, Address> pair = pool_[bucket].back();
  pool_[bucket].pop_back();
  return pair;
}

PageBackend::PageBackend(PageAllocator* allocator) : allocator_(allocator) {}

PageBackend::~PageBackend() = default;

Address PageBackend::AllocateNormalPageMemory(size_t bucket) {
  std::pair<NormalPageMemoryRegion*, Address> result = page_pool_.Take(bucket);
  if (!result.first) {
    auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator_);
    for (size_t i = 0; i < NormalPageMemoryRegion::kNumPageRegions; ++i) {
      page_pool_.Add(bucket, pmr.get(),
                     pmr->GetPageMemory(i).writeable_region().base());
    }
    page_memory_region_tree_.Add(pmr.get());
    normal_page_memory_regions_.push_back(std::move(pmr));
    return AllocateNormalPageMemory(bucket);
  }
  result.first->Allocate(result.second);
  return result.second;
}

void PageBackend::FreeNormalPageMemory(size_t bucket, Address writeable_base) {
  auto* pmr = static_cast<NormalPageMemoryRegion*>(
      page_memory_region_tree_.Lookup(writeable_base));
  pmr->Free(writeable_base);
  page_pool_.Add(bucket, pmr, writeable_base);
}

Address PageBackend::AllocateLargePageMemory(size_t size) {
  auto pmr = std::make_unique<LargePageMemoryRegion>(allocator_, size);
  const PageMemory pm = pmr->GetPageMemory();
  Unprotect(allocator_, pm);
  page_memory_region_tree_.Add(pmr.get());
  large_page_memory_regions_.insert(std::make_pair(pmr.get(), std::move(pmr)));
  return pm.writeable_region().base();
}

void PageBackend::FreeLargePageMemory(Address writeable_base) {
  PageMemoryRegion* pmr = page_memory_region_tree_.Lookup(writeable_base);
  page_memory_region_tree_.Remove(pmr);
  auto size = large_page_memory_regions_.erase(pmr);
  USE(size);
  DCHECK_EQ(1u, size);
}

}  // namespace internal
}  // namespace cppgc
