// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/emulated-virtual-address-subspace.h"

#include "src/base/bits.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"

namespace v8 {
namespace base {

EmulatedVirtualAddressSubspace::EmulatedVirtualAddressSubspace(
    VirtualAddressSpace* parent_space, Address base, size_t mapped_size,
    size_t total_size)
    : VirtualAddressSpace(parent_space->page_size(),
                          parent_space->allocation_granularity(), base,
                          total_size),
      mapped_size_(mapped_size),
      parent_space_(parent_space),
      region_allocator_(base, mapped_size, parent_space_->page_size()) {
  // For simplicity, we currently require both the mapped and total size to be
  // a power of two. This simplifies some things later on, for example, random
  // addresses can be generated with a simply bitmask, and will then be inside
  // the unmapped space with a probability >= 50% (mapped size == unmapped
  // size) or never (mapped size == total size).
  DCHECK(base::bits::IsPowerOfTwo(mapped_size));
  DCHECK(base::bits::IsPowerOfTwo(total_size));
}

EmulatedVirtualAddressSubspace::~EmulatedVirtualAddressSubspace() {
  CHECK(parent_space_->FreePages(base(), mapped_size_));
}

void EmulatedVirtualAddressSubspace::SetRandomSeed(int64_t seed) {
  MutexGuard guard(&mutex_);
  rng_.SetSeed(seed);
}

Address EmulatedVirtualAddressSubspace::RandomPageAddress() {
  MutexGuard guard(&mutex_);
  Address addr = base() + (rng_.NextInt64() % size());
  return RoundDown(addr, allocation_granularity());
}

Address EmulatedVirtualAddressSubspace::AllocatePages(
    Address hint, size_t size, size_t alignment, PagePermissions permissions) {
  if (hint == kNoHint || MappedRegionContains(hint, size)) {
    MutexGuard guard(&mutex_);

    // Attempt to find a region in the mapped region.
    Address address = region_allocator_.AllocateRegion(hint, size, alignment);
    if (address != RegionAllocator::kAllocationFailure) {
      // Success. Only need to adjust the page permissions.
      if (parent_space_->SetPagePermissions(address, size, permissions)) {
        return address;
      }
      // Probably ran out of memory, but still try to allocate in the unmapped
      // space.
      CHECK_EQ(size, region_allocator_.FreeRegion(address));
    }
  }

  // No luck or hint is outside of the mapped region. Try to allocate pages in
  // the unmapped space using page allocation hints instead.

  // Somewhat arbitrary size limitation to ensure that the loop below for
  // finding a fitting base address hint terminates quickly.
  if (size >= (unmapped_size() / 2)) return kNullAddress;

  static constexpr int kMaxAttempts = 10;
  for (int i = 0; i < kMaxAttempts; i++) {
    // If the hint wouldn't result in the entire allocation being inside the
    // managed region, simply retry. There is at least a 50% chance of
    // getting a usable address due to the size restriction above.
    while (!UnmappedRegionContains(hint, size)) {
      hint = RandomPageAddress();
    }

    Address region =
        parent_space_->AllocatePages(hint, size, alignment, permissions);
    if (region && UnmappedRegionContains(region, size)) {
      return region;
    } else if (region) {
      CHECK(parent_space_->FreePages(region, size));
    }

    // Retry at a different address.
    hint = RandomPageAddress();
  }

  return kNullAddress;
}

bool EmulatedVirtualAddressSubspace::FreePages(Address address, size_t size) {
  if (MappedRegionContains(address, size)) {
    MutexGuard guard(&mutex_);
    if (region_allocator_.FreeRegion(address) != size) return false;
    CHECK(parent_space_->DecommitPages(address, size));
    return true;
  }
  if (!UnmappedRegionContains(address, size)) return false;
  return parent_space_->FreePages(address, size);
}

bool EmulatedVirtualAddressSubspace::SetPagePermissions(
    Address address, size_t size, PagePermissions permissions) {
  DCHECK(Contains(address, size));
  return parent_space_->SetPagePermissions(address, size, permissions);
}

bool EmulatedVirtualAddressSubspace::CanAllocateSubspaces() {
  // This is not supported, mostly because it's not (yet) needed in practice.
  return false;
}

std::unique_ptr<v8::VirtualAddressSpace>
EmulatedVirtualAddressSubspace::AllocateSubspace(
    Address hint, size_t size, size_t alignment,
    PagePermissions max_permissions) {
  UNREACHABLE();
}

bool EmulatedVirtualAddressSubspace::DiscardSystemPages(Address address,
                                                        size_t size) {
  DCHECK(Contains(address, size));
  return parent_space_->DiscardSystemPages(address, size);
}

bool EmulatedVirtualAddressSubspace::DecommitPages(Address address,
                                                   size_t size) {
  DCHECK(Contains(address, size));
  return parent_space_->DecommitPages(address, size);
}

}  // namespace base
}  // namespace v8
