// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/zx/resource.h>
#include <lib/zx/thread.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

namespace {

zx_vm_option_t GetProtectionFromMemoryPermission(OS::MemoryPermission access) {
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

// Determine ZX_VM_ALIGN_X constant corresponding to the specified alignment.
// Returns 0 if there is none.
zx_vm_option_t GetAlignmentOptionFromAlignment(size_t alignment) {
  // The alignment must be one of the ZX_VM_ALIGN_X constants.
  // See zircon/system/public/zircon/types.h.
  static_assert(
      ZX_VM_ALIGN_1KB == (10 << ZX_VM_ALIGN_BASE),
      "Fuchsia's ZX_VM_ALIGN_1KB constant doesn't match expected value");
  static_assert(
      ZX_VM_ALIGN_4GB == (32 << ZX_VM_ALIGN_BASE),
      "Fuchsia's ZX_VM_ALIGN_4GB constant doesn't match expected value");
  zx_vm_option_t alignment_log2 = 0;
  for (int shift = 10; shift <= 32; shift++) {
    if (alignment == (size_t{1} << shift)) {
      alignment_log2 = shift;
      break;
    }
  }
  return alignment_log2 << ZX_VM_ALIGN_BASE;
}

void* AllocateInternal(const zx::vmar& vmar, size_t page_size,
                       size_t vmar_offset, bool vmar_offset_is_hint,
                       size_t size, size_t alignment,
                       OS::MemoryPermission access) {
  DCHECK_EQ(0, size % page_size);
  DCHECK_EQ(0, alignment % page_size);
  DCHECK_EQ(0, vmar_offset % page_size);

  zx::vmo vmo;
  if (zx::vmo::create(size, 0, &vmo) != ZX_OK) {
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

  zx_vm_option_t options = GetProtectionFromMemoryPermission(access);

  zx_vm_option_t alignment_option = GetAlignmentOptionFromAlignment(alignment);
  CHECK_NE(0, alignment_option);  // Invalid alignment specified
  options |= alignment_option;

  if (vmar_offset != 0) {
    options |= ZX_VM_SPECIFIC;
  }

  zx_vaddr_t address;
  zx_status_t status = vmar.map(options, vmar_offset, vmo, 0, size, &address);

  if (status != ZX_OK && vmar_offset != 0 && vmar_offset_is_hint) {
    // If a vmar_offset was specified and the allocation failed (for example,
    // because the offset overlapped another mapping), then we should retry
    // again without a vmar_offset if that offset was just meant to be a hint.
    options &= ~(ZX_VM_SPECIFIC);
    status = vmar.map(options, 0, vmo, 0, size, &address);
  }

  if (status != ZX_OK) {
    return nullptr;
  }

  return reinterpret_cast<void*>(address);
}

bool FreeInternal(const zx::vmar& vmar, size_t page_size, void* address,
                  const size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % page_size);
  DCHECK_EQ(0, size % page_size);
  return vmar.unmap(reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

bool SetPermissionsInternal(const zx::vmar& vmar, size_t page_size,
                            void* address, size_t size,
                            OS::MemoryPermission access) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % page_size);
  DCHECK_EQ(0, size % page_size);
  uint32_t prot = GetProtectionFromMemoryPermission(access);
  return vmar.protect(prot, reinterpret_cast<uintptr_t>(address), size) ==
         ZX_OK;
}

bool DiscardSystemPagesInternal(const zx::vmar& vmar, size_t page_size,
                                void* address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % page_size);
  DCHECK_EQ(0, size % page_size);
  uint64_t address_int = reinterpret_cast<uint64_t>(address);
  return vmar.op_range(ZX_VMO_OP_DECOMMIT, address_int, size, nullptr, 0) ==
         ZX_OK;
}

zx_status_t CreateAddressSpaceReservationInternal(
    const zx::vmar& vmar, size_t page_size, size_t vmar_offset,
    bool vmar_offset_is_hint, size_t size, size_t alignment,
    OS::MemoryPermission max_permission, zx::vmar* child,
    zx_vaddr_t* child_addr) {
  DCHECK_EQ(0, size % page_size);
  DCHECK_EQ(0, alignment % page_size);
  DCHECK_EQ(0, vmar_offset % page_size);

  // TODO(v8) determine these based on max_permission.
  zx_vm_option_t options = ZX_VM_CAN_MAP_READ | ZX_VM_CAN_MAP_WRITE |
                           ZX_VM_CAN_MAP_EXECUTE | ZX_VM_CAN_MAP_SPECIFIC;

  zx_vm_option_t alignment_option = GetAlignmentOptionFromAlignment(alignment);
  CHECK_NE(0, alignment_option);  // Invalid alignment specified
  options |= alignment_option;

  if (vmar_offset != 0) {
    options |= ZX_VM_SPECIFIC;
  }

  zx_status_t status =
      vmar.allocate(options, vmar_offset, size, child, child_addr);
  if (status != ZX_OK && vmar_offset != 0 && vmar_offset_is_hint) {
    // If a vmar_offset was specified and the allocation failed (for example,
    // because the offset overlapped another mapping), then we should retry
    // again without a vmar_offset if that offset was just meant to be a hint.
    options &= ~(ZX_VM_SPECIFIC);
    status = vmar.allocate(options, 0, size, child, child_addr);
  }

  return status;
}

}  // namespace

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

// static
void* OS::Allocate(void* address, size_t size, size_t alignment,
                   MemoryPermission access) {
  constexpr bool vmar_offset_is_hint = true;
  DCHECK_EQ(0, reinterpret_cast<Address>(address) % alignment);
  return AllocateInternal(*zx::vmar::root_self(), AllocatePageSize(),
                          reinterpret_cast<uint64_t>(address),
                          vmar_offset_is_hint, size, alignment, access);
}

// static
bool OS::Free(void* address, const size_t size) {
  return FreeInternal(*zx::vmar::root_self(), AllocatePageSize(), address,
                      size);
}

// static
bool OS::Release(void* address, size_t size) { return Free(address, size); }

// static
bool OS::SetPermissions(void* address, size_t size, MemoryPermission access) {
  return SetPermissionsInternal(*zx::vmar::root_self(), CommitPageSize(),
                                address, size, access);
}

// static
bool OS::DiscardSystemPages(void* address, size_t size) {
  return DiscardSystemPagesInternal(*zx::vmar::root_self(), CommitPageSize(),
                                    address, size);
}

// static
bool OS::DecommitPages(void* address, size_t size) {
  // We rely on DiscardSystemPages decommitting the pages immediately (via
  // ZX_VMO_OP_DECOMMIT) so that they are guaranteed to be zero-initialized
  // should they be accessed again later on.
  return SetPermissions(address, size, MemoryPermission::kNoAccess) &&
         DiscardSystemPages(address, size);
}

// static
bool OS::CanReserveAddressSpace() { return true; }

// static
Optional<AddressSpaceReservation> OS::CreateAddressSpaceReservation(
    void* hint, size_t size, size_t alignment,
    MemoryPermission max_permission) {
  DCHECK_EQ(0, reinterpret_cast<Address>(hint) % alignment);
  zx::vmar child;
  zx_vaddr_t child_addr;
  uint64_t vmar_offset = reinterpret_cast<uint64_t>(hint);
  constexpr bool vmar_offset_is_hint = true;
  zx_status_t status = CreateAddressSpaceReservationInternal(
      *zx::vmar::root_self(), AllocatePageSize(), vmar_offset,
      vmar_offset_is_hint, size, alignment, max_permission, &child,
      &child_addr);
  if (status != ZX_OK) return {};
  return AddressSpaceReservation(reinterpret_cast<void*>(child_addr), size,
                                 child.release());
}

// static
bool OS::FreeAddressSpaceReservation(AddressSpaceReservation reservation) {
  // Destroy the vmar and release the handle.
  zx::vmar vmar(reservation.vmar_);
  return vmar.destroy() == ZX_OK;
}

// static
bool OS::HasLazyCommits() { return true; }

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

std::vector<OS::MemoryRange> OS::GetFreeMemoryRangesWithin(
    OS::Address boundary_start, OS::Address boundary_end, size_t minimum_size,
    size_t alignment) {
  return {};
}

Optional<AddressSpaceReservation> AddressSpaceReservation::CreateSubReservation(
    void* address, size_t size, OS::MemoryPermission max_permission) {
  DCHECK(Contains(address, size));

  zx::vmar child;
  zx_vaddr_t child_addr;
  size_t vmar_offset = 0;
  if (address != 0) {
    vmar_offset =
        reinterpret_cast<size_t>(address) - reinterpret_cast<size_t>(base());
  }
  constexpr bool vmar_offset_is_hint = false;
  zx_status_t status = CreateAddressSpaceReservationInternal(
      *zx::unowned_vmar(vmar_), OS::AllocatePageSize(), vmar_offset,
      vmar_offset_is_hint, size, OS::AllocatePageSize(), max_permission, &child,
      &child_addr);
  if (status != ZX_OK) return {};
  DCHECK_EQ(reinterpret_cast<void*>(child_addr), address);
  return AddressSpaceReservation(reinterpret_cast<void*>(child_addr), size,
                                 child.release());
}

bool AddressSpaceReservation::FreeSubReservation(
    AddressSpaceReservation reservation) {
  return OS::FreeAddressSpaceReservation(reservation);
}

bool AddressSpaceReservation::Allocate(void* address, size_t size,
                                       OS::MemoryPermission access) {
  DCHECK(Contains(address, size));
  size_t vmar_offset = 0;
  if (address != 0) {
    vmar_offset =
        reinterpret_cast<size_t>(address) - reinterpret_cast<size_t>(base());
  }
  constexpr bool vmar_offset_is_hint = false;
  void* allocation = AllocateInternal(
      *zx::unowned_vmar(vmar_), OS::AllocatePageSize(), vmar_offset,
      vmar_offset_is_hint, size, OS::AllocatePageSize(), access);
  DCHECK(!allocation || allocation == address);
  return allocation != nullptr;
}

bool AddressSpaceReservation::Free(void* address, size_t size) {
  DCHECK(Contains(address, size));
  return FreeInternal(*zx::unowned_vmar(vmar_), OS::AllocatePageSize(), address,
                      size);
}

bool AddressSpaceReservation::SetPermissions(void* address, size_t size,
                                             OS::MemoryPermission access) {
  DCHECK(Contains(address, size));
  return SetPermissionsInternal(*zx::unowned_vmar(vmar_), OS::CommitPageSize(),
                                address, size, access);
}

bool AddressSpaceReservation::DiscardSystemPages(void* address, size_t size) {
  DCHECK(Contains(address, size));
  return DiscardSystemPagesInternal(*zx::unowned_vmar(vmar_),
                                    OS::CommitPageSize(), address, size);
}

bool AddressSpaceReservation::DecommitPages(void* address, size_t size) {
  DCHECK(Contains(address, size));
  // See comment in OS::DecommitPages.
  return SetPermissions(address, size, OS::MemoryPermission::kNoAccess) &&
         DiscardSystemPages(address, size);
}

}  // namespace base
}  // namespace v8
