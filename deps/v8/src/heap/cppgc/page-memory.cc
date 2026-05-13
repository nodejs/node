// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/page-memory.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "include/v8config.h"
#include "src/base/macros.h"
#include "src/base/sanitizer/asan.h"
#include "src/heap/cppgc/memory.h"
#include "src/heap/cppgc/platform.h"

#if V8_OS_POSIX
#include <errno.h>
#endif

namespace cppgc {
namespace internal {

namespace {

V8_WARN_UNUSED_RESULT bool TryUnprotect(PageAllocator& allocator,
                                        const MemoryRegion& memory_region) {
  // The allocator needs to support committing the overall range.
  CHECK_EQ(0u, memory_region.size() % allocator.CommitPageSize());
  return allocator.SetPermissions(memory_region.base(), memory_region.size(),
                                  PageAllocator::Permission::kReadWrite);
}

std::optional<MemoryRegion> ReserveMemoryRegion(PageAllocator& allocator,
                                                size_t allocation_size) {
  void* region_memory =
      allocator.AllocatePages(nullptr, allocation_size, kPageSize,
                              PageAllocator::Permission::kNoAccess);
  if (!region_memory) {
    return std::nullopt;
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

std::unique_ptr<PageMemoryRegion> CreateNormalPageMemoryRegion(
    PageAllocator& allocator) {
  DCHECK_EQ(0u, kPageSize % allocator.AllocatePageSize());
  const auto region = ReserveMemoryRegion(allocator, kPageSize);
  if (!region) return {};
  auto result = std::unique_ptr<PageMemoryRegion>(
      new PageMemoryRegion(allocator, *region));
  return result;
}

std::unique_ptr<PageMemoryRegion> CreateLargePageMemoryRegion(
    PageAllocator& allocator, size_t length) {
  const auto region = ReserveMemoryRegion(
      allocator, RoundUp(length, allocator.AllocatePageSize()));
  if (!region) return {};
  auto result = std::unique_ptr<PageMemoryRegion>(
      new PageMemoryRegion(allocator, *region));
  return result;
}

}  // namespace

PageMemoryRegion::PageMemoryRegion(PageAllocator& allocator,
                                   MemoryRegion reserved_region)
    : allocator_(allocator), reserved_region_(reserved_region) {}

PageMemoryRegion::~PageMemoryRegion() {
  FreeMemoryRegion(allocator_, region());
}

void PageMemoryRegion::UnprotectForTesting() {
  CHECK(TryUnprotect(allocator_, region()));
}

PageMemoryRegionTree::PageMemoryRegionTree() = default;

PageMemoryRegionTree::~PageMemoryRegionTree() = default;

void PageMemoryRegionTree::Add(PageMemoryRegion* region) {
  DCHECK(region);
  const auto result = set_.emplace(region->region().base(), region);
  USE(result);
  DCHECK(result.second);
}

void PageMemoryRegionTree::Remove(PageMemoryRegion* region) {
  DCHECK(region);
  const auto size = set_.erase(region->region().base());
  USE(size);
  DCHECK_EQ(1u, size);
}

void NormalPageMemoryPool::Add(PageMemoryRegion* pmr) {
  DCHECK_NOT_NULL(pmr);
  DCHECK_EQ(pmr->region().size(), kPageSize);
  // Oilpan requires the pages to be zero-initialized.
  {
    void* base = pmr->region().base();
    const size_t size = pmr->region().size();
    AsanUnpoisonScope unpoison_for_memset(base, size);
    std::memset(base, 0, size);
  }
  pool_.push_back(pmr);
}

PageMemoryRegion* NormalPageMemoryPool::Take() {
  if (pool_.empty()) return nullptr;
  PageMemoryRegion* pmr = pool_.back();
  DCHECK_NOT_NULL(pmr);
  pool_.pop_back();
  void* base = pmr->region().base();
  const size_t size = pmr->region().size();
  ASAN_UNPOISON_MEMORY_REGION(base, size);

#if DEBUG
  CheckMemoryIsZero(base, size);
#endif
  return pmr;
}

size_t NormalPageMemoryPool::PooledMemory() const {
  size_t total_size = 0;
  for (auto* pmr : pool_) {
    total_size += pmr->region().size();
  }
  return total_size;
}

std::vector<PageMemoryRegion*> NormalPageMemoryPool::TakeAll() {
  return std::move(pool_);
}

PageBackend::PageBackend(PageAllocator& normal_page_allocator,
                         PageAllocator& large_page_allocator)
    : normal_page_allocator_(normal_page_allocator),
      large_page_allocator_(large_page_allocator) {}

PageBackend::~PageBackend() = default;

Address PageBackend::TryAllocateNormalPageMemory() {
  v8::base::MutexGuard guard(&mutex_);
  if (PageMemoryRegion* cached = page_pool_.Take()) {
    const auto region = cached->region();
    DCHECK_NE(normal_page_memory_regions_.end(),
              normal_page_memory_regions_.find(cached));
    page_memory_region_tree_.Add(cached);
    return region.base();
  }
  auto pmr = CreateNormalPageMemoryRegion(normal_page_allocator_);
  if (!pmr) {
    return nullptr;
  }
  const auto memory_region = pmr->region();
  if (V8_LIKELY(TryUnprotect(normal_page_allocator_, memory_region))) {
    page_memory_region_tree_.Add(pmr.get());
    normal_page_memory_regions_.emplace(pmr.get(), std::move(pmr));
    return memory_region.base();
  }
  return nullptr;
}

void PageBackend::FreeNormalPageMemory(Address writeable_base) {
  v8::base::MutexGuard guard(&mutex_);
  auto* pmr = page_memory_region_tree_.Lookup(writeable_base);
  DCHECK_NOT_NULL(pmr);
  page_memory_region_tree_.Remove(pmr);
  page_pool_.Add(pmr);
}

Address PageBackend::TryAllocateLargePageMemory(size_t size) {
  v8::base::MutexGuard guard(&mutex_);
  auto pmr = CreateLargePageMemoryRegion(large_page_allocator_, size);
  if (!pmr) {
    return nullptr;
  }
  const auto memory_region = pmr->region();
  if (V8_LIKELY(TryUnprotect(large_page_allocator_, memory_region))) {
    page_memory_region_tree_.Add(pmr.get());
    large_page_memory_regions_.emplace(pmr.get(), std::move(pmr));
    return memory_region.base();
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

void PageBackend::ReleasePooledPages() {
  v8::base::MutexGuard guard(&mutex_);
  auto pooled_regions = page_pool_.TakeAll();
  for (auto* region : pooled_regions) {
    auto erased_count = normal_page_memory_regions_.erase(region);
    USE(erased_count);
    DCHECK_EQ(1u, erased_count);
  }
}

}  // namespace internal
}  // namespace cppgc
