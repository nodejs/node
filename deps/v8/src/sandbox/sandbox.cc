// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/sandbox.h"

#include "include/v8-internal.h"
#include "src/base/bits.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/cpu.h"
#include "src/base/emulated-virtual-address-subspace.h"
#include "src/base/lazy-instance.h"
#include "src/base/utils/random-number-generator.h"
#include "src/base/virtual-address-space-page-allocator.h"
#include "src/base/virtual-address-space.h"
#include "src/flags/flags.h"
#include "src/sandbox/sandboxed-pointer.h"
#include "src/utils/allocation.h"

#if defined(V8_OS_WIN)
#include <windows.h>
// This has to come after windows.h.
#include <versionhelpers.h>  // For IsWindows8Point1OrGreater().
#endif

namespace v8 {
namespace internal {

#ifdef V8_SANDBOX_IS_AVAILABLE

// Best-effort helper function to determine the size of the userspace virtual
// address space. Used to determine appropriate sandbox size and placement.
static Address DetermineAddressSpaceLimit() {
#ifndef V8_TARGET_ARCH_64_BIT
#error Unsupported target architecture.
#endif

  // Assume 48 bits by default, which seems to be the most common configuration.
  constexpr unsigned kDefaultVirtualAddressBits = 48;
  // 36 bits should realistically be the lowest value we could ever see.
  constexpr unsigned kMinVirtualAddressBits = 36;
  constexpr unsigned kMaxVirtualAddressBits = 64;

  constexpr size_t kMinVirtualAddressSpaceSize = 1ULL << kMinVirtualAddressBits;
  static_assert(kMinVirtualAddressSpaceSize >= kSandboxMinimumSize,
                "The minimum sandbox size should be smaller or equal to the "
                "smallest possible userspace address space. Otherwise, large "
                "parts of the sandbox will not be usable on those platforms.");

#ifdef V8_TARGET_ARCH_X64
  base::CPU cpu;
  Address virtual_address_bits = kDefaultVirtualAddressBits;
  if (cpu.exposes_num_virtual_address_bits()) {
    virtual_address_bits = cpu.num_virtual_address_bits();
  }
#else
  // TODO(saelo) support ARM and possibly other CPUs as well.
  Address virtual_address_bits = kDefaultVirtualAddressBits;
#endif

  // Guard against nonsensical values.
  if (virtual_address_bits < kMinVirtualAddressBits ||
      virtual_address_bits > kMaxVirtualAddressBits) {
    virtual_address_bits = kDefaultVirtualAddressBits;
  }

  // Assume virtual address space is split 50/50 between userspace and kernel.
  Address userspace_virtual_address_bits = virtual_address_bits - 1;
  Address address_space_limit = 1ULL << userspace_virtual_address_bits;

#if defined(V8_OS_WIN_X64)
  if (!IsWindows8Point1OrGreater()) {
    // On Windows pre 8.1 userspace is limited to 8TB on X64. See
    // https://docs.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases
    address_space_limit = 8ULL * TB;
  }
#endif  // V8_OS_WIN_X64

  // TODO(saelo) we could try allocating memory in the upper half of the address
  // space to see if it is really usable.
  return address_space_limit;
}

bool Sandbox::Initialize(v8::VirtualAddressSpace* vas) {
  // Take the number of virtual address bits into account when determining the
  // size of the sandbox. For example, if there are only 39 bits available,
  // split evenly between userspace and kernel, then userspace can only address
  // 256GB and so we use a quarter of that, 64GB, as maximum size.
  Address address_space_limit = DetermineAddressSpaceLimit();
  size_t max_sandbox_size = address_space_limit / 4;
  size_t sandbox_size = std::min(kSandboxSize, max_sandbox_size);
  size_t size_to_reserve = sandbox_size;

  // If the size is less than the minimum sandbox size though, we fall back to
  // creating a partially reserved sandbox, as that allows covering more virtual
  // address space. This happens for CPUs with only 36 virtual address bits, in
  // which case the sandbox size would end up being only 8GB.
  bool partially_reserve = false;
  if (sandbox_size < kSandboxMinimumSize) {
    static_assert(
        (8ULL * GB) >= kSandboxMinimumReservationSize,
        "Minimum reservation size for a partially reserved sandbox must be at "
        "most 8GB to support CPUs with only 36 virtual address bits");
    size_to_reserve = sandbox_size;
    sandbox_size = kSandboxMinimumSize;
    partially_reserve = true;
  }

#if defined(V8_OS_WIN)
  if (!IsWindows8Point1OrGreater()) {
    // On Windows pre 8.1, reserving virtual memory is an expensive operation,
    // apparently because the OS already charges for the memory required for
    // all page table entries. For example, a 1TB reservation increases private
    // memory usage by 2GB. As such, it is not possible to create a proper
    // sandbox there and so a partially reserved sandbox is created which
    // doesn't reserve most of the virtual memory, and so doesn't incur the
    // cost, but also doesn't provide the desired security benefits.
    size_to_reserve = kSandboxMinimumReservationSize;
    partially_reserve = true;
  }
#endif  // V8_OS_WIN

  if (!vas->CanAllocateSubspaces()) {
    // If we cannot create virtual memory subspaces, we also need to fall back
    // to creating a partially reserved sandbox. In practice, this should only
    // happen on Windows version before Windows 10, maybe including early
    // Windows 10 releases, where the necessary memory management APIs, in
    // particular, VirtualAlloc2, are not available. This check should also in
    // practice subsume the preceeding one for Windows 8 and earlier, but we'll
    // keep both just to be sure since there the partially reserved sandbox is
    // technically required for a different reason (large virtual memory
    // reservations being too expensive).
    size_to_reserve = kSandboxMinimumReservationSize;
    partially_reserve = true;
  }

  // In any case, the sandbox must be at most as large as our address space.
  DCHECK_LE(sandbox_size, address_space_limit);

  if (partially_reserve) {
    return InitializeAsPartiallyReservedSandbox(vas, sandbox_size,
                                                size_to_reserve);
  } else {
    const bool use_guard_regions = true;
    bool success = Initialize(vas, sandbox_size, use_guard_regions);
#ifdef V8_SANDBOXED_POINTERS
    // If sandboxed pointers are enabled, we need the sandbox to be initialized,
    // so fall back to creating a partially reserved sandbox.
    if (!success) {
      // Instead of going for the minimum reservation size directly, we could
      // also first try a couple of larger reservation sizes if that is deemed
      // sensible in the future.
      success = InitializeAsPartiallyReservedSandbox(
          vas, sandbox_size, kSandboxMinimumReservationSize);
    }
#endif  // V8_SANDBOXED_POINTERS
    return success;
  }
}

bool Sandbox::Initialize(v8::VirtualAddressSpace* vas, size_t size,
                         bool use_guard_regions) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK_GE(size, kSandboxMinimumSize);
  CHECK(vas->CanAllocateSubspaces());

  // Currently, we allow the sandbox to be smaller than the requested size.
  // This way, we can gracefully handle address space reservation failures
  // during the initial rollout and can collect data on how often these occur.
  // In the future, we will likely either require the sandbox to always have a
  // fixed size or will design SandboxedPointers (pointers that are guaranteed
  // to point into the sandbox) in a way that doesn't reduce the sandbox's
  // security properties if it has a smaller size.  Which of these options is
  // ultimately taken likey depends on how frequently sandbox reservation
  // failures occur in practice.
  size_t reservation_size;
  while (!address_space_ && size >= kSandboxMinimumSize) {
    reservation_size = size;
    if (use_guard_regions) {
      reservation_size += 2 * kSandboxGuardRegionSize;
    }

    Address hint = RoundDown(vas->RandomPageAddress(), kSandboxAlignment);

    // There should be no executable pages mapped inside the sandbox since
    // those could be corrupted by an attacker and therefore pose a security
    // risk. Furthermore, allowing executable mappings in the sandbox requires
    // MAP_JIT on macOS, which causes fork() to become excessively slow
    // (multiple seconds or even minutes for a 1TB sandbox on macOS 12.X), in
    // turn causing tests to time out. As such, the maximum page permission
    // inside the sandbox should be read + write.
    address_space_ = vas->AllocateSubspace(
        hint, reservation_size, kSandboxAlignment, PagePermissions::kReadWrite);
    if (!address_space_) {
      size /= 2;
    }
  }

  if (!address_space_) return false;

  reservation_base_ = address_space_->base();
  base_ = reservation_base_;
  if (use_guard_regions) {
    base_ += kSandboxGuardRegionSize;
  }

  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = reservation_size;

  if (use_guard_regions) {
    Address front = reservation_base_;
    Address back = end_;
    // These must succeed since nothing was allocated in the subspace yet.
    CHECK(address_space_->AllocateGuardRegion(front, kSandboxGuardRegionSize));
    CHECK(address_space_->AllocateGuardRegion(back, kSandboxGuardRegionSize));
  }

  sandbox_page_allocator_ =
      std::make_unique<base::VirtualAddressSpacePageAllocator>(
          address_space_.get());

  initialized_ = true;
  is_partially_reserved_ = false;

  InitializeConstants();

  return true;
}

bool Sandbox::InitializeAsPartiallyReservedSandbox(v8::VirtualAddressSpace* vas,
                                                   size_t size,
                                                   size_t size_to_reserve) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK(base::bits::IsPowerOfTwo(size_to_reserve));
  CHECK_GE(size, kSandboxMinimumSize);
  CHECK_LT(size_to_reserve, size);

  // Use a custom random number generator here to ensure that we get uniformly
  // distributed random numbers. We figure out the available address space
  // ourselves, and so are potentially better positioned to determine a good
  // base address for the sandbox than the embedder.
  base::RandomNumberGenerator rng;
  if (FLAG_random_seed != 0) {
    rng.SetSeed(FLAG_random_seed);
  }

  // We try to ensure that base + size is still (mostly) within the process'
  // address space, even though we only reserve a fraction of the memory. For
  // that, we attempt to map the sandbox into the first half of the usable
  // address space. This keeps the implementation simple and should, In any
  // realistic scenario, leave plenty of space after the actual reservation.
  Address address_space_end = DetermineAddressSpaceLimit();
  Address highest_allowed_address = address_space_end / 2;
  DCHECK(base::bits::IsPowerOfTwo(highest_allowed_address));
  constexpr int kMaxAttempts = 10;
  for (int i = 1; i <= kMaxAttempts; i++) {
    Address hint = rng.NextInt64() % highest_allowed_address;
    hint = RoundDown(hint, kSandboxAlignment);

    reservation_base_ = vas->AllocatePages(
        hint, size_to_reserve, kSandboxAlignment, PagePermissions::kNoAccess);

    if (!reservation_base_) return false;

    // Take this base if it meets the requirements or if this is the last
    // attempt.
    if (reservation_base_ <= highest_allowed_address || i == kMaxAttempts)
      break;

    // Can't use this base, so free the reservation and try again
    vas->FreePages(reservation_base_, size_to_reserve);
    reservation_base_ = kNullAddress;
  }
  DCHECK(reservation_base_);

  base_ = reservation_base_;
  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = size_to_reserve;
  initialized_ = true;
  is_partially_reserved_ = true;
  address_space_ = std::make_unique<base::EmulatedVirtualAddressSubspace>(
      vas, reservation_base_, reservation_size_, size_);
  sandbox_page_allocator_ =
      std::make_unique<base::VirtualAddressSpacePageAllocator>(
          address_space_.get());

  InitializeConstants();

  return true;
}

void Sandbox::InitializeConstants() {
#ifdef V8_SANDBOXED_POINTERS
  // Place the empty backing store buffer at the end of the sandbox, so that any
  // accidental access to it will most likely hit a guard page.
  constants_.set_empty_backing_store_buffer(base_ + size_ - 1);
#endif
}

void Sandbox::TearDown() {
  if (initialized_) {
    // This destroys the sub space and frees the underlying reservation.
    address_space_.reset();
    sandbox_page_allocator_.reset();
    base_ = kNullAddress;
    end_ = kNullAddress;
    size_ = 0;
    reservation_base_ = kNullAddress;
    reservation_size_ = 0;
    initialized_ = false;
    is_partially_reserved_ = false;
#ifdef V8_SANDBOXED_POINTERS
    constants_.Reset();
#endif
  }
  disabled_ = false;
}

#endif  // V8_SANDBOX_IS_AVAILABLE

#ifdef V8_SANDBOX
DEFINE_LAZY_LEAKY_OBJECT_GETTER(Sandbox, GetProcessWideSandbox)
#endif

}  // namespace internal
}  // namespace v8
