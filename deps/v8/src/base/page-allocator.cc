// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/page-allocator.h"

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum: " #a)

STATIC_ASSERT_ENUM(PageAllocator::kNoAccess,
                   base::OS::MemoryPermission::kNoAccess);
STATIC_ASSERT_ENUM(PageAllocator::kReadWrite,
                   base::OS::MemoryPermission::kReadWrite);
STATIC_ASSERT_ENUM(PageAllocator::kReadWriteExecute,
                   base::OS::MemoryPermission::kReadWriteExecute);
STATIC_ASSERT_ENUM(PageAllocator::kReadExecute,
                   base::OS::MemoryPermission::kReadExecute);

#undef STATIC_ASSERT_ENUM

size_t PageAllocator::AllocatePageSize() {
  return base::OS::AllocatePageSize();
}

size_t PageAllocator::CommitPageSize() { return base::OS::CommitPageSize(); }

void PageAllocator::SetRandomMmapSeed(int64_t seed) {
  base::OS::SetRandomMmapSeed(seed);
}

void* PageAllocator::GetRandomMmapAddr() {
  return base::OS::GetRandomMmapAddr();
}

void* PageAllocator::AllocatePages(void* address, size_t size, size_t alignment,
                                   PageAllocator::Permission access) {
  return base::OS::Allocate(address, size, alignment,
                            static_cast<base::OS::MemoryPermission>(access));
}

bool PageAllocator::FreePages(void* address, size_t size) {
  return base::OS::Free(address, size);
}

bool PageAllocator::ReleasePages(void* address, size_t size, size_t new_size) {
  DCHECK_LT(new_size, size);
  return base::OS::Release(reinterpret_cast<uint8_t*>(address) + new_size,
                           size - new_size);
}

bool PageAllocator::SetPermissions(void* address, size_t size,
                                   PageAllocator::Permission access) {
  return base::OS::SetPermissions(
      address, size, static_cast<base::OS::MemoryPermission>(access));
}

}  // namespace base
}  // namespace v8
