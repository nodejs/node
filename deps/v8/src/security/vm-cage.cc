// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/security/vm-cage.h"

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
#include "src/security/caged-pointer.h"
#include "src/utils/allocation.h"

#if defined(V8_OS_WIN)
#include <windows.h>
// This has to come after windows.h.
#include <versionhelpers.h>  // For IsWindows8Point1OrGreater().
#endif

namespace v8 {
namespace internal {

#ifdef V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

// Best-effort helper function to determine the size of the userspace virtual
// address space. Used to determine appropriate cage size and placement.
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
  static_assert(kMinVirtualAddressSpaceSize >= kVirtualMemoryCageMinimumSize,
                "The minimum cage size should be smaller or equal to the "
                "smallest possible userspace address space. Otherwise, larger "
                "parts of the cage will not be usable on those platforms.");

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

bool V8VirtualMemoryCage::Initialize(v8::VirtualAddressSpace* vas) {
  // Take the number of virtual address bits into account when determining the
  // size of the cage. For example, if there are only 39 bits available, split
  // evenly between userspace and kernel, then userspace can only address 256GB
  // and so we use a quarter of that, 64GB, as maximum cage size.
  Address address_space_limit = DetermineAddressSpaceLimit();
  size_t max_cage_size = address_space_limit / 4;
  size_t cage_size = std::min(kVirtualMemoryCageSize, max_cage_size);
  size_t size_to_reserve = cage_size;

  // If the size is less than the minimum cage size though, we fall back to
  // creating a fake cage. This happens for CPUs with only 36 virtual address
  // bits, in which case the cage size would end up being only 8GB.
  bool create_fake_cage = false;
  if (cage_size < kVirtualMemoryCageMinimumSize) {
    static_assert((8ULL * GB) >= kFakeVirtualMemoryCageMinReservationSize,
                  "Minimum reservation size for a fake cage must be at most "
                  "8GB to support CPUs with only 36 virtual address bits");
    size_to_reserve = cage_size;
    cage_size = kVirtualMemoryCageMinimumSize;
    create_fake_cage = true;
  }

#if defined(V8_OS_WIN)
  if (!IsWindows8Point1OrGreater()) {
    // On Windows pre 8.1, reserving virtual memory is an expensive operation,
    // apparently because the OS already charges for the memory required for
    // all page table entries. For example, a 1TB reservation increases private
    // memory usage by 2GB. As such, it is not possible to create a proper
    // virtual memory cage there and so a fake cage is created which doesn't
    // reserve most of the virtual memory, and so doesn't incur the cost, but
    // also doesn't provide the desired security benefits.
    size_to_reserve = kFakeVirtualMemoryCageMinReservationSize;
    create_fake_cage = true;
  }
#endif  // V8_OS_WIN

  if (!vas->CanAllocateSubspaces()) {
    // If we cannot create virtual memory subspaces, we also need to fall back
    // to creating a fake cage. In practice, this should only happen on Windows
    // version before Windows 10, maybe including early Windows 10 releases,
    // where the necessary memory management APIs, in particular, VirtualAlloc2,
    // are not available. This check should also in practice subsume the
    // preceeding one for Windows 8 and earlier, but we'll keep both just to be
    // sure since there the fake cage is technically required for a different
    // reason (large virtual memory reservations being too expensive).
    size_to_reserve = kFakeVirtualMemoryCageMinReservationSize;
    create_fake_cage = true;
  }

  // In any case, the (fake) cage must be at most as large as our address space.
  DCHECK_LE(cage_size, address_space_limit);

  if (create_fake_cage) {
    return InitializeAsFakeCage(vas, cage_size, size_to_reserve);
  } else {
    // TODO(saelo) if this fails, we could still fall back to creating a fake
    // cage. Decide if that would make sense.
    const bool use_guard_regions = true;
    return Initialize(vas, cage_size, use_guard_regions);
  }
}

bool V8VirtualMemoryCage::Initialize(v8::VirtualAddressSpace* vas, size_t size,
                                     bool use_guard_regions) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK_GE(size, kVirtualMemoryCageMinimumSize);
  CHECK(vas->CanAllocateSubspaces());

  // Currently, we allow the cage to be smaller than the requested size. This
  // way, we can gracefully handle cage reservation failures during the initial
  // rollout and can collect data on how often these occur. In the future, we
  // will likely either require the cage to always have a fixed size or will
  // design CagedPointers (pointers that are guaranteed to point into the cage,
  // e.g. because they are stored as offsets from the cage base) in a way that
  // doesn't reduce the cage's security properties if it has a smaller size.
  // Which of these options is ultimately taken likey depends on how frequently
  // cage reservation failures occur in practice.
  size_t reservation_size;
  while (!virtual_address_space_ && size >= kVirtualMemoryCageMinimumSize) {
    reservation_size = size;
    if (use_guard_regions) {
      reservation_size += 2 * kVirtualMemoryCageGuardRegionSize;
    }

    Address hint =
        RoundDown(vas->RandomPageAddress(), kVirtualMemoryCageAlignment);

    // Currently, executable memory is still allocated inside the cage. In the
    // future, we should drop that and use kReadWrite as max_permissions.
    virtual_address_space_ = vas->AllocateSubspace(
        hint, reservation_size, kVirtualMemoryCageAlignment,
        PagePermissions::kReadWriteExecute);
    if (!virtual_address_space_) {
      size /= 2;
    }
  }

  if (!virtual_address_space_) return false;

  reservation_base_ = virtual_address_space_->base();
  base_ = reservation_base_;
  if (use_guard_regions) {
    base_ += kVirtualMemoryCageGuardRegionSize;
  }

  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = reservation_size;

  if (use_guard_regions) {
    // These must succeed since nothing was allocated in the subspace yet.
    CHECK_EQ(reservation_base_,
             virtual_address_space_->AllocatePages(
                 reservation_base_, kVirtualMemoryCageGuardRegionSize,
                 vas->allocation_granularity(), PagePermissions::kNoAccess));
    CHECK_EQ(end_,
             virtual_address_space_->AllocatePages(
                 end_, kVirtualMemoryCageGuardRegionSize,
                 vas->allocation_granularity(), PagePermissions::kNoAccess));
  }

  cage_page_allocator_ =
      std::make_unique<base::VirtualAddressSpacePageAllocator>(
          virtual_address_space_.get());

  initialized_ = true;
  is_fake_cage_ = false;

  InitializeConstants();

  return true;
}

bool V8VirtualMemoryCage::InitializeAsFakeCage(v8::VirtualAddressSpace* vas,
                                               size_t size,
                                               size_t size_to_reserve) {
  CHECK(!initialized_);
  CHECK(!disabled_);
  CHECK(base::bits::IsPowerOfTwo(size));
  CHECK(base::bits::IsPowerOfTwo(size_to_reserve));
  CHECK_GE(size, kVirtualMemoryCageMinimumSize);
  CHECK_LT(size_to_reserve, size);

  // Use a custom random number generator here to ensure that we get uniformly
  // distributed random numbers. We figure out the available address space
  // ourselves, and so are potentially better positioned to determine a good
  // base address for the cage than the embedder.
  base::RandomNumberGenerator rng;
  if (FLAG_random_seed != 0) {
    rng.SetSeed(FLAG_random_seed);
  }

  // We try to ensure that base + size is still (mostly) within the process'
  // address space, even though we only reserve a fraction of the memory. For
  // that, we attempt to map the cage into the first half of the usable address
  // space. This keeps the implementation simple and should, In any realistic
  // scenario, leave plenty of space after the cage reservation.
  Address address_space_end = DetermineAddressSpaceLimit();
  Address highest_allowed_address = address_space_end / 2;
  DCHECK(base::bits::IsPowerOfTwo(highest_allowed_address));
  constexpr int kMaxAttempts = 10;
  for (int i = 1; i <= kMaxAttempts; i++) {
    Address hint = rng.NextInt64() % highest_allowed_address;
    hint = RoundDown(hint, kVirtualMemoryCageAlignment);

    reservation_base_ =
        vas->AllocatePages(hint, size_to_reserve, kVirtualMemoryCageAlignment,
                           PagePermissions::kNoAccess);

    if (!reservation_base_) return false;

    // Take this base if it meets the requirements or if this is the last
    // attempt.
    if (reservation_base_ <= highest_allowed_address || i == kMaxAttempts)
      break;

    // Can't use this base, so free the reservation and try again
    CHECK(vas->FreePages(reservation_base_, size_to_reserve));
    reservation_base_ = kNullAddress;
  }
  DCHECK(reservation_base_);

  base_ = reservation_base_;
  size_ = size;
  end_ = base_ + size_;
  reservation_size_ = size_to_reserve;
  initialized_ = true;
  is_fake_cage_ = true;
  virtual_address_space_ =
      std::make_unique<base::EmulatedVirtualAddressSubspace>(
          vas, reservation_base_, reservation_size_, size_);
  cage_page_allocator_ =
      std::make_unique<base::VirtualAddressSpacePageAllocator>(
          virtual_address_space_.get());

  InitializeConstants();

  return true;
}

void V8VirtualMemoryCage::InitializeConstants() {
#ifdef V8_CAGED_POINTERS
  // Place the empty backing store buffer at the end of the cage, so that any
  // accidental access to it will most likely hit a guard page.
  constants_.set_empty_backing_store_buffer(base_ + size_ - 1);
#endif
}

void V8VirtualMemoryCage::TearDown() {
  if (initialized_) {
    // This destroys the sub space and frees the underlying reservation.
    virtual_address_space_.reset();
    cage_page_allocator_.reset();
    base_ = kNullAddress;
    end_ = kNullAddress;
    size_ = 0;
    reservation_base_ = kNullAddress;
    reservation_size_ = 0;
    initialized_ = false;
    is_fake_cage_ = false;
#ifdef V8_CAGED_POINTERS
    constants_.Reset();
#endif
  }
  disabled_ = false;
}

#endif  // V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

#ifdef V8_VIRTUAL_MEMORY_CAGE
DEFINE_LAZY_LEAKY_OBJECT_GETTER(V8VirtualMemoryCage,
                                GetProcessWideVirtualMemoryCage)
#endif

}  // namespace internal
}  // namespace v8
