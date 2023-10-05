// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/page-memory.h"

#include <cstddef>

#include "src/base/macros.h"
#include "src/base/sanitizer/asan.h"
#include "src/heap/cppgc/platform.h"

namespace cppgc {
namespace internal {

namespace {

V8_WARN_UNUSED_RESULT bool TryUnprotect(PageAllocator& allocator,
                                        const PageMemory& page_memory) {
  if (SupportsCommittingGuardPages(allocator)) {
    return allocator.SetPermissions(page_memory.writeable_region().base(),
                                    page_memory.writeable_region().size(),
                                    PageAllocator::Permission::kReadWrite);
  }
  // No protection using guard pages in case the allocator cannot commit at
  // the required granularity. Only protect if the allocator supports
  // committing at that granularity.
  //
  // The allocator needs to support committing the overall range.
  CHECK_EQ(0u,
           page_memory.overall_region().size() % allocator.CommitPageSize());
  return allocator.SetPermissions(page_memory.overall_region().base(),
                                  page_memory.overall_region().size(),
                                  PageAllocator::Permission::kReadWrite);
}

V8_WARN_UNUSED_RESULT bool TryProtect(PageAllocator& allocator,
                                      const PageMemory& page_memory) {
  if (SupportsCommittingGuardPages(allocator)) {
    // Swap the same region, providing the OS with a chance for fast lookup and
    // change.
    return allocator.SetPermissions(page_memory.writeable_region().base(),
                                    page_memory.writeable_region().size(),
                                    PageAllocator::Permission::kNoAccess);
  }
  // See Unprotect().
  CHECK_EQ(0u,
           page_memory.overall_region().size() % allocator.CommitPageSize());
  return allocator.SetPermissions(page_memory.overall_region().base(),
                                  page_memory.overall_region().size(),
                                  PageAllocator::Permission::kNoAccess);
}

v8::base::Optional<MemoryRegion> ReserveMemoryRegion(PageAllocator& allocator,
                                                     size_t allocation_size) {
  void* region_memory =
      allocator.AllocatePages(nullptr, allocation_size, kPageSize,
                              PageAllocator::Permission::kNoAccess);
  if (!region_memory) {
    return v8::base::nullopt;
  }
  const MemoryRegion reserved_region(static_cast<Address>(region_memory),
                                     allocation_size);
  DCHECK_EQ(reserved_region.base() + allocation_size, reserved_region.end());
  return reserved_region;
}

void FreeMemoryRegion(PageAllocator& allocator,
                      const MemoryRegion& reserved_region) {
  // Make sure pages returned to OS are unpoisoned.
  ASAN_UNPOISON_MEMORY_REGION(reserved_region.base(), reserved_region.size());
  allocator.FreePages(reserved_region.base(), reserved_region.size());
}

}  // namespace

PageMemoryRegion::PageMemoryRegion(PageAllocator& allocator,
                                   MemoryRegion reserved_region, bool is_large)
    : allocator_(allocator),
      reserved_region_(reserved_region),
      is_large_(is_large) {}

PageMemoryRegion::~PageMemoryRegion() {
  FreeMemoryRegion(allocator_, reserved_region());
}

// static
constexpr size_t NormalPageMemoryRegion::kNumPageRegions;

// static
std::unique_ptr<NormalPageMemoryRegion> NormalPageMemoryRegion::Create(
    PageAllocator& allocator) {
  const auto region = ReserveMemoryRegion(
      allocator,
      RoundUp(kPageSize * kNumPageRegions, allocator.AllocatePageSize()));
  if (!region) return {};
  auto result = std::unique_ptr<NormalPageMemoryRegion>(
      new NormalPageMemoryRegion(allocator, *region));
  return result;
}

NormalPageMemoryRegion::NormalPageMemoryRegion(PageAllocator& allocator,
                                               MemoryRegion region)
    : PageMemoryRegion(allocator, region, false) {
#ifdef DEBUG
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    DCHECK_EQ(false, page_memories_in_use_[i]);
  }
#endif  // DEBUG
}

NormalPageMemoryRegion::~NormalPageMemoryRegion() = default;

bool NormalPageMemoryRegion::TryAllocate(Address writeable_base) {
  const size_t index = GetIndex(writeable_base);
  if (TryUnprotect(allocator_, GetPageMemory(index))) {
    ChangeUsed(index, true);
    return true;
  }
  return false;
}

void NormalPageMemoryRegion::Free(Address writeable_base) {
  const size_t index = GetIndex(writeable_base);
  ChangeUsed(index, false);
  CHECK(TryProtect(allocator_, GetPageMemory(index)));
}

void NormalPageMemoryRegion::UnprotectForTesting() {
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    CHECK(TryUnprotect(allocator_, GetPageMemory(i)));
  }
}

// static
std::unique_ptr<LargePageMemoryRegion> LargePageMemoryRegion::Create(
    PageAllocator& allocator, size_t length) {
  const auto region = ReserveMemoryRegion(
      allocator,
      RoundUp(length + 2 * kGuardPageSize, allocator.AllocatePageSize()));
  if (!region) return {};
  auto result = std::unique_ptr<LargePageMemoryRegion>(
      new LargePageMemoryRegion(allocator, *region));
  return result;
}

LargePageMemoryRegion::LargePageMemoryRegion(PageAllocator& allocator,
                                             MemoryRegion region)
    : PageMemoryRegion(allocator, region, true) {}

LargePageMemoryRegion::~LargePageMemoryRegion() = default;

void LargePageMemoryRegion::UnprotectForTesting() {
  CHECK(TryUnprotect(allocator_, GetPageMemory()));
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

void NormalPageMemoryPool::Add(NormalPageMemoryRegion* pmr,
                               Address writeable_base) {
  pool_.push_back(std::make_pair(pmr, writeable_base));
}

std::pair<NormalPageMemoryRegion*, Address> NormalPageMemoryPool::Take() {
  if (pool_.empty()) return {nullptr, nullptr};
  std::pair<NormalPageMemoryRegion*, Address> pair = pool_.back();
  pool_.pop_back();
  return pair;
}

PageBackend::PageBackend(PageAllocator& normal_page_allocator,
                         PageAllocator& large_page_allocator)
    : normal_page_allocator_(normal_page_allocator),
      large_page_allocator_(large_page_allocator) {}

PageBackend::~PageBackend() = default;

Address PageBackend::TryAllocateNormalPageMemory() {
  v8::base::MutexGuard guard(&mutex_);
  std::pair<NormalPageMemoryRegion*, Address> result = page_pool_.Take();
  if (!result.first) {
    auto pmr = NormalPageMemoryRegion::Create(normal_page_allocator_);
    if (!pmr) {
      return nullptr;
    }
    for (size_t i = 0; i < NormalPageMemoryRegion::kNumPageRegions; ++i) {
      page_pool_.Add(pmr.get(),
                     pmr->GetPageMemory(i).writeable_region().base());
    }
    page_memory_region_tree_.Add(pmr.get());
    normal_page_memory_regions_.push_back(std::move(pmr));
    result = page_pool_.Take();
    DCHECK(result.first);
  }
  if (V8_LIKELY(result.first->TryAllocate(result.second))) {
    return result.second;
  }
  page_pool_.Add(result.first, result.second);
  return nullptr;
}

void PageBackend::FreeNormalPageMemory(size_t bucket, Address writeable_base) {
  v8::base::MutexGuard guard(&mutex_);
  auto* pmr = static_cast<NormalPageMemoryRegion*>(
      page_memory_region_tree_.Lookup(writeable_base));
  pmr->Free(writeable_base);
  page_pool_.Add(pmr, writeable_base);
}

Address PageBackend::TryAllocateLargePageMemory(size_t size) {
  v8::base::MutexGuard guard(&mutex_);
  auto pmr = LargePageMemoryRegion::Create(large_page_allocator_, size);
  if (!pmr) {
    return nullptr;
  }
  const PageMemory pm = pmr->GetPageMemory();
  if (V8_LIKELY(TryUnprotect(large_page_allocator_, pm))) {
    page_memory_region_tree_.Add(pmr.get());
    large_page_memory_regions_.insert(
        std::make_pair(pmr.get(), std::move(pmr)));
    return pm.writeable_region().base();
  }
  return nullptr;
}

void PageBackend::FreeLargePageMemory(Address writeable_base) {
  v8::base::MutexGuard guard(&mutex_);
  PageMemoryRegion* pmr = page_memory_region_tree_.Lookup(writeable_base);
  page_memory_region_tree_.Remove(pmr);
  auto size = large_page_memory_regions_.erase(pmr);
  USE(size);
  DCHECK_EQ(1u, size);
}

}  // namespace internal
}  // namespace cppgc
