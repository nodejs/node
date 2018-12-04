// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

namespace {

uint32_t GetProtectionFromMemoryPermission(OS::MemoryPermission access) {
  switch (access) {
    case OS::MemoryPermission::kNoAccess:
      return 0;  // no permissions
    case OS::MemoryPermission::kRead:
      return ZX_VM_FLAG_PERM_READ;
    case OS::MemoryPermission::kReadWrite:
      return ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE;
    case OS::MemoryPermission::kReadWriteExecute:
      return ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE |
             ZX_VM_FLAG_PERM_EXECUTE;
    case OS::MemoryPermission::kReadExecute:
      return ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_EXECUTE;
  }
  UNREACHABLE();
}

}  // namespace

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

// static
void* OS::Allocate(void* address, size_t size, size_t alignment,
                   OS::MemoryPermission access) {
  size_t page_size = OS::AllocatePageSize();
  DCHECK_EQ(0, size % page_size);
  DCHECK_EQ(0, alignment % page_size);
  address = AlignedAddress(address, alignment);
  // Add the maximum misalignment so we are guaranteed an aligned base address.
  size_t request_size = size + (alignment - page_size);

  zx_handle_t vmo;
  if (zx_vmo_create(request_size, 0, &vmo) != ZX_OK) {
    return nullptr;
  }
  static const char kVirtualMemoryName[] = "v8-virtualmem";
  zx_object_set_property(vmo, ZX_PROP_NAME, kVirtualMemoryName,
                         strlen(kVirtualMemoryName));
  uintptr_t reservation;
  uint32_t prot = GetProtectionFromMemoryPermission(access);
  zx_status_t status = zx_vmar_map(zx_vmar_root_self(), prot, 0, vmo, 0,
                                   request_size, &reservation);
  // Either the vmo is now referenced by the vmar, or we failed and are bailing,
  // so close the vmo either way.
  zx_handle_close(vmo);
  if (status != ZX_OK) {
    return nullptr;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(reservation);
  uint8_t* aligned_base = reinterpret_cast<uint8_t*>(
      RoundUp(reinterpret_cast<uintptr_t>(base), alignment));

  // Unmap extra memory reserved before and after the desired block.
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    zx_vmar_unmap(zx_vmar_root_self(), reinterpret_cast<uintptr_t>(base),
                  prefix_size);
    request_size -= prefix_size;
  }

  size_t aligned_size = RoundUp(size, page_size);

  if (aligned_size != request_size) {
    DCHECK_LT(aligned_size, request_size);
    size_t suffix_size = request_size - aligned_size;
    zx_vmar_unmap(zx_vmar_root_self(),
                  reinterpret_cast<uintptr_t>(aligned_base + aligned_size),
                  suffix_size);
    request_size -= suffix_size;
  }

  DCHECK(aligned_size == request_size);
  return static_cast<void*>(aligned_base);
}

// static
bool OS::Free(void* address, const size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % AllocatePageSize());
  DCHECK_EQ(0, size % AllocatePageSize());
  return zx_vmar_unmap(zx_vmar_root_self(),
                       reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

// static
bool OS::Release(void* address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % CommitPageSize());
  DCHECK_EQ(0, size % CommitPageSize());
  return zx_vmar_unmap(zx_vmar_root_self(),
                       reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

// static
bool OS::SetPermissions(void* address, size_t size, MemoryPermission access) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % CommitPageSize());
  DCHECK_EQ(0, size % CommitPageSize());
  uint32_t prot = GetProtectionFromMemoryPermission(access);
  return zx_vmar_protect(zx_vmar_root_self(), prot,
                         reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

// static
bool OS::HasLazyCommits() {
  // TODO(scottmg): Port, https://crbug.com/731217.
  return false;
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  UNREACHABLE();  // TODO(scottmg): Port, https://crbug.com/731217.
}

void OS::SignalCodeMovingGC() {
  UNREACHABLE();  // TODO(scottmg): Port, https://crbug.com/731217.
}

int OS::GetUserTime(uint32_t* secs, uint32_t* usecs) {
  const auto kNanosPerMicrosecond = 1000ULL;
  const auto kMicrosPerSecond = 1000000ULL;
  const zx_time_t nanos_since_thread_started = zx_clock_get(ZX_CLOCK_THREAD);

  // First convert to microseconds, rounding up.
  const uint64_t micros_since_thread_started =
      (nanos_since_thread_started + kNanosPerMicrosecond - 1ULL) /
      kNanosPerMicrosecond;

  *secs = static_cast<uint32_t>(micros_since_thread_started / kMicrosPerSecond);
  *usecs =
      static_cast<uint32_t>(micros_since_thread_started % kMicrosPerSecond);
  return 0;
}

}  // namespace base
}  // namespace v8
