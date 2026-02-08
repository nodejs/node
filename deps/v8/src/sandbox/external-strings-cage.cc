// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/external-strings-cage.h"

#include <stddef.h>

#include "include/v8-internal.h"
#include "include/v8-platform.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/build_config.h"
#include "src/base/logging.h"
#include "src/base/sanitizer/asan.h"
#include "src/base/sanitizer/msan.h"
#include "src/base/virtual-address-space.h"
#include "src/utils/allocation.h"

namespace v8::internal {

#if defined(V8_ENABLE_SANDBOX) && defined(V8_ENABLE_MEMORY_CORRUPTION_API)

ExternalStringsCage::ExternalStringsCage()
    : page_size_(GetPlatformPageAllocator()->AllocatePageSize()) {
  CHECK_EQ(kMaxContentsSize % page_size_, 0);
  CHECK_EQ(kGuardRegionSize % page_size_, 0);
}

ExternalStringsCage::~ExternalStringsCage() = default;

bool ExternalStringsCage::Initialize() {
  VirtualMemoryCage::ReservationParams params = {
      .page_allocator = GetPlatformPageAllocator(),
      .reservation_size = kMaxContentsSize + kGuardRegionSize,
      .base_alignment = VirtualMemoryCage::ReservationParams::kAnyBaseAlignment,
      .page_size = page_size_,
      .requested_start_hint = kNullAddress,
      .permissions = PageAllocator::Permission::kNoAccess,
      .page_initialization_mode =
          base::PageInitializationMode::kAllocatedPagesCanBeUninitialized,
      .page_freeing_mode = base::PageFreeingMode::kMakeInaccessible,
  };
  if (!vm_cage_.InitReservation(params)) return false;

  Address guard_region_begin = vm_cage_.base() + kMaxContentsSize;
  CHECK(vm_cage_.page_allocator()->AllocatePagesAt(
      guard_region_begin, kGuardRegionSize, PageAllocator::kNoAccess));
  return true;
}

void ExternalStringsCage::Seal(void* ptr, size_t size) {
  size_t alloc_size = GetAllocSize(size);
  vm_cage_.page_allocator()->SetPermissions(ptr, alloc_size,
                                            PageAllocator::kRead);
  // TODO(crbug/360048056): Also use the seal syscall when it's supported.
}

size_t ExternalStringsCage::GetAllocSize(size_t string_size) const {
  CHECK_GT(string_size, 0);
  CHECK_LE(string_size, kMaxContentsSize);
  // Allocate whole pages as we're relying on BoundedPageAllocator (e.g.,
  // PartitionAlloc would allow smaller granularity, but at the moment it's not
  // possible as we'd need a second Configurable Pool).
  size_t page_count = (string_size + page_size_ - 1) / page_size_;
  return page_count * page_size_;
}

void* ExternalStringsCage::AllocateRaw(size_t size) {
  if (!size) return nullptr;
  size_t alloc_size = GetAllocSize(size);
  void* ptr = vm_cage_.page_allocator()->AllocatePages(
      nullptr, alloc_size, page_size_, PageAllocator::kReadWrite);
  CHECK(ptr);
  CHECK_LE(reinterpret_cast<Address>(ptr) + size,
           vm_cage_.base() + kMaxContentsSize);
#if defined(V8_USE_MEMORY_SANITIZER)
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(ptr, alloc_size);
#elif defined(V8_USE_ADDRESS_SANITIZER)
  void* redzone_begin =
      reinterpret_cast<void*>(reinterpret_cast<Address>(ptr) + size);
  size_t redzone_size = alloc_size - size;
  ASAN_POISON_MEMORY_REGION(redzone_begin, redzone_size);
#endif
  return ptr;
}

void ExternalStringsCage::Free(void* ptr, size_t size) {
  if (!ptr) {
    CHECK_EQ(size, 0);
    return;
  }
  size_t alloc_size = GetAllocSize(size);
#if defined(V8_USE_ADDRESS_SANITIZER)
  // Unpoison the memory before giving it back (as it may be reused later).
  ASAN_UNPOISON_MEMORY_REGION(ptr, alloc_size);
#endif
  vm_cage_.page_allocator()->FreePages(ptr, alloc_size);
}

#endif  // defined(V8_ENABLE_SANDBOX) &&
        // defined(V8_ENABLE_MEMORY_CORRUPTION_API)

}  // namespace v8::internal
