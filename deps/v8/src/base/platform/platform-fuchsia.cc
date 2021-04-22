// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/zx/resource.h>
#include <lib/zx/thread.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

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
    case OS::MemoryPermission::kNoAccessWillJitLater:
      return 0;  // no permissions
    case OS::MemoryPermission::kRead:
      return ZX_VM_PERM_READ;
    case OS::MemoryPermission::kReadWrite:
      return ZX_VM_PERM_READ | ZX_VM_PERM_WRITE;
    case OS::MemoryPermission::kReadWriteExecute:
      return ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE;
    case OS::MemoryPermission::kReadExecute:
      return ZX_VM_PERM_READ | ZX_VM_PERM_EXECUTE;
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

  zx::vmo vmo;
  if (zx::vmo::create(request_size, 0, &vmo) != ZX_OK) {
    return nullptr;
  }
  static const char kVirtualMemoryName[] = "v8-virtualmem";
  vmo.set_property(ZX_PROP_NAME, kVirtualMemoryName,
                   strlen(kVirtualMemoryName));

  // Always call zx_vmo_replace_as_executable() in case the memory will need
  // to be marked as executable in the future.
  // TOOD(https://crbug.com/v8/8899): Only call this when we know that the
  // region will need to be marked as executable in the future.
  if (vmo.replace_as_executable(zx::resource(), &vmo) != ZX_OK) {
    return nullptr;
  }

  zx_vaddr_t reservation;
  uint32_t prot = GetProtectionFromMemoryPermission(access);
  if (zx::vmar::root_self()->map(prot, 0, vmo, 0, request_size, &reservation) !=
      ZX_OK) {
    return nullptr;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(reservation);
  uint8_t* aligned_base = reinterpret_cast<uint8_t*>(
      RoundUp(reinterpret_cast<uintptr_t>(base), alignment));

  // Unmap extra memory reserved before and after the desired block.
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    zx::vmar::root_self()->unmap(reinterpret_cast<uintptr_t>(base),
                                 prefix_size);
    request_size -= prefix_size;
  }

  size_t aligned_size = RoundUp(size, page_size);

  if (aligned_size != request_size) {
    DCHECK_LT(aligned_size, request_size);
    size_t suffix_size = request_size - aligned_size;
    zx::vmar::root_self()->unmap(
        reinterpret_cast<uintptr_t>(aligned_base + aligned_size), suffix_size);
    request_size -= suffix_size;
  }

  DCHECK(aligned_size == request_size);
  return static_cast<void*>(aligned_base);
}

// static
bool OS::Free(void* address, const size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % AllocatePageSize());
  DCHECK_EQ(0, size % AllocatePageSize());
  return zx::vmar::root_self()->unmap(reinterpret_cast<uintptr_t>(address),
                                      size) == ZX_OK;
}

// static
bool OS::Release(void* address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % CommitPageSize());
  DCHECK_EQ(0, size % CommitPageSize());
  return zx::vmar::root_self()->unmap(reinterpret_cast<uintptr_t>(address),
                                      size) == ZX_OK;
}

// static
bool OS::SetPermissions(void* address, size_t size, MemoryPermission access) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % CommitPageSize());
  DCHECK_EQ(0, size % CommitPageSize());
  uint32_t prot = GetProtectionFromMemoryPermission(access);
  return zx::vmar::root_self()->protect2(
             prot, reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

// static
bool OS::DiscardSystemPages(void* address, size_t size) {
  // TODO(hpayer): Does Fuchsia have madvise?
  return true;
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

  zx_info_thread_stats_t info = {};
  if (zx::thread::self()->get_info(ZX_INFO_THREAD_STATS, &info, sizeof(info),
                                   nullptr, nullptr) != ZX_OK) {
    return -1;
  }

  // First convert to microseconds, rounding up.
  const uint64_t micros_since_thread_started =
      (info.total_runtime + kNanosPerMicrosecond - 1ULL) / kNanosPerMicrosecond;

  *secs = static_cast<uint32_t>(micros_since_thread_started / kMicrosPerSecond);
  *usecs =
      static_cast<uint32_t>(micros_since_thread_started % kMicrosPerSecond);
  return 0;
}

void OS::AdjustSchedulingParams() {}

}  // namespace base
}  // namespace v8
