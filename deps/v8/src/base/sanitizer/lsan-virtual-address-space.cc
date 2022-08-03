// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/sanitizer/lsan-virtual-address-space.h"

#include "include/v8-platform.h"
#include "src/base/logging.h"

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace v8 {
namespace base {

LsanVirtualAddressSpace::LsanVirtualAddressSpace(
    std::unique_ptr<v8::VirtualAddressSpace> vas)
    : VirtualAddressSpace(vas->page_size(), vas->allocation_granularity(),
                          vas->base(), vas->size(),
                          vas->max_page_permissions()),
      vas_(std::move(vas)) {
  DCHECK_NOT_NULL(vas_);
}

Address LsanVirtualAddressSpace::AllocatePages(Address hint, size_t size,
                                               size_t alignment,
                                               PagePermissions permissions) {
  Address result = vas_->AllocatePages(hint, size, alignment, permissions);
#if defined(LEAK_SANITIZER)
  if (result) {
    __lsan_register_root_region(reinterpret_cast<void*>(result), size);
  }
#endif  // defined(LEAK_SANITIZER)
  return result;
}

void LsanVirtualAddressSpace::FreePages(Address address, size_t size) {
  vas_->FreePages(address, size);
#if defined(LEAK_SANITIZER)
  __lsan_unregister_root_region(reinterpret_cast<void*>(address), size);
#endif  // defined(LEAK_SANITIZER)
}

Address LsanVirtualAddressSpace::AllocateSharedPages(
    Address hint, size_t size, PagePermissions permissions,
    PlatformSharedMemoryHandle handle, uint64_t offset) {
  Address result =
      vas_->AllocateSharedPages(hint, size, permissions, handle, offset);
#if defined(LEAK_SANITIZER)
  if (result) {
    __lsan_register_root_region(reinterpret_cast<void*>(result), size);
  }
#endif  // defined(LEAK_SANITIZER)
  return result;
}

void LsanVirtualAddressSpace::FreeSharedPages(Address address, size_t size) {
  vas_->FreeSharedPages(address, size);
#if defined(LEAK_SANITIZER)
  __lsan_unregister_root_region(reinterpret_cast<void*>(address), size);
#endif  // defined(LEAK_SANITIZER)
}

std::unique_ptr<VirtualAddressSpace> LsanVirtualAddressSpace::AllocateSubspace(
    Address hint, size_t size, size_t alignment,
    PagePermissions max_page_permissions) {
  auto subspace =
      vas_->AllocateSubspace(hint, size, alignment, max_page_permissions);
#if defined(LEAK_SANITIZER)
  if (subspace) {
    subspace = std::make_unique<LsanVirtualAddressSpace>(std::move(subspace));
  }
#endif  // defined(LEAK_SANITIZER)
  return subspace;
}

}  // namespace base
}  // namespace v8
