// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/virtual-memory.h"

#include "include/cppgc/platform.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

VirtualMemory::VirtualMemory(PageAllocator* page_allocator, size_t size,
                             size_t alignment, void* hint)
    : page_allocator_(page_allocator) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK(IsAligned(size, page_allocator->CommitPageSize()));

  const size_t page_size = page_allocator_->AllocatePageSize();
  start_ = page_allocator->AllocatePages(hint, RoundUp(size, page_size),
                                         RoundUp(alignment, page_size),
                                         PageAllocator::kNoAccess);
  if (start_) {
    size_ = RoundUp(size, page_size);
  }
}

VirtualMemory::~VirtualMemory() V8_NOEXCEPT {
  if (IsReserved()) {
    page_allocator_->FreePages(start_, size_);
  }
}

VirtualMemory::VirtualMemory(VirtualMemory&& other) V8_NOEXCEPT
    : page_allocator_(std::move(other.page_allocator_)),
      start_(std::move(other.start_)),
      size_(std::move(other.size_)) {
  other.Reset();
}

VirtualMemory& VirtualMemory::operator=(VirtualMemory&& other) V8_NOEXCEPT {
  DCHECK(!IsReserved());
  page_allocator_ = std::move(other.page_allocator_);
  start_ = std::move(other.start_);
  size_ = std::move(other.size_);
  other.Reset();
  return *this;
}

void VirtualMemory::Reset() {
  start_ = nullptr;
  size_ = 0;
}

}  // namespace internal
}  // namespace cppgc
