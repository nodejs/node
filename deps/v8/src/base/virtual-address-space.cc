// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/virtual-address-space.h"

#include "include/v8-platform.h"
#include "src/base/bits.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"

namespace v8 {
namespace base {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum: " #a)

STATIC_ASSERT_ENUM(PagePermissions::kNoAccess, OS::MemoryPermission::kNoAccess);
STATIC_ASSERT_ENUM(PagePermissions::kReadWrite,
                   OS::MemoryPermission::kReadWrite);
STATIC_ASSERT_ENUM(PagePermissions::kReadWriteExecute,
                   OS::MemoryPermission::kReadWriteExecute);
STATIC_ASSERT_ENUM(PagePermissions::kReadExecute,
                   OS::MemoryPermission::kReadExecute);

#undef STATIC_ASSERT_ENUM

VirtualAddressSpace::VirtualAddressSpace()
    : VirtualAddressSpaceBase(OS::CommitPageSize(), OS::AllocatePageSize(),
                              kNullAddress,
                              std::numeric_limits<uintptr_t>::max()) {
#if V8_OS_WIN
  // On Windows, this additional step is required to lookup the VirtualAlloc2
  // and friends functions.
  OS::EnsureWin32MemoryAPILoaded();
#endif  // V8_OS_WIN
  DCHECK(bits::IsPowerOfTwo(page_size()));
  DCHECK(bits::IsPowerOfTwo(allocation_granularity()));
  DCHECK_GE(allocation_granularity(), page_size());
  DCHECK(IsAligned(allocation_granularity(), page_size()));
}

void VirtualAddressSpace::SetRandomSeed(int64_t seed) {
  OS::SetRandomMmapSeed(seed);
}

Address VirtualAddressSpace::RandomPageAddress() {
  return reinterpret_cast<Address>(OS::GetRandomMmapAddr());
}

Address VirtualAddressSpace::AllocatePages(Address hint, size_t size,
                                           size_t alignment,
                                           PagePermissions permissions) {
  DCHECK(IsAligned(alignment, allocation_granularity()));
  DCHECK(IsAligned(hint, alignment));
  DCHECK(IsAligned(size, allocation_granularity()));

  return reinterpret_cast<Address>(
      OS::Allocate(reinterpret_cast<void*>(hint), size, alignment,
                   static_cast<OS::MemoryPermission>(permissions)));
}

bool VirtualAddressSpace::FreePages(Address address, size_t size) {
  DCHECK(IsAligned(address, allocation_granularity()));
  DCHECK(IsAligned(size, allocation_granularity()));

  return OS::Free(reinterpret_cast<void*>(address), size);
}

bool VirtualAddressSpace::SetPagePermissions(Address address, size_t size,
                                             PagePermissions permissions) {
  DCHECK(IsAligned(address, page_size()));
  DCHECK(IsAligned(size, page_size()));

  return OS::SetPermissions(reinterpret_cast<void*>(address), size,
                            static_cast<OS::MemoryPermission>(permissions));
}

bool VirtualAddressSpace::CanAllocateSubspaces() {
  return OS::CanReserveAddressSpace();
}

std::unique_ptr<v8::VirtualAddressSpace> VirtualAddressSpace::AllocateSubspace(
    Address hint, size_t size, size_t alignment,
    PagePermissions max_permissions) {
  DCHECK(IsAligned(alignment, allocation_granularity()));
  DCHECK(IsAligned(hint, alignment));
  DCHECK(IsAligned(size, allocation_granularity()));

  base::Optional<AddressSpaceReservation> reservation =
      OS::CreateAddressSpaceReservation(
          reinterpret_cast<void*>(hint), size, alignment,
          static_cast<OS::MemoryPermission>(max_permissions));
  if (!reservation.has_value())
    return std::unique_ptr<v8::VirtualAddressSpace>();
  return std::unique_ptr<v8::VirtualAddressSpace>(
      new VirtualAddressSubspace(*reservation, this));
}

bool VirtualAddressSpace::DiscardSystemPages(Address address, size_t size) {
  DCHECK(IsAligned(address, page_size()));
  DCHECK(IsAligned(size, page_size()));

  return OS::DiscardSystemPages(reinterpret_cast<void*>(address), size);
}

bool VirtualAddressSpace::DecommitPages(Address address, size_t size) {
  DCHECK(IsAligned(address, page_size()));
  DCHECK(IsAligned(size, page_size()));

  return OS::DecommitPages(reinterpret_cast<void*>(address), size);
}

bool VirtualAddressSpace::FreeSubspace(VirtualAddressSubspace* subspace) {
  return OS::FreeAddressSpaceReservation(subspace->reservation_);
}

VirtualAddressSubspace::VirtualAddressSubspace(
    AddressSpaceReservation reservation, VirtualAddressSpaceBase* parent_space)
    : VirtualAddressSpaceBase(
          parent_space->page_size(), parent_space->allocation_granularity(),
          reinterpret_cast<Address>(reservation.base()), reservation.size()),
      reservation_(reservation),
      region_allocator_(reinterpret_cast<Address>(reservation.base()),
                        reservation.size(),
                        parent_space->allocation_granularity()),
      parent_space_(parent_space) {
#if V8_OS_WIN
  // On Windows, the address space reservation needs to be split and merged at
  // the OS level as well.
  region_allocator_.set_on_split_callback([this](Address start, size_t size) {
    DCHECK(IsAligned(start, allocation_granularity()));
    CHECK(reservation_.SplitPlaceholder(reinterpret_cast<void*>(start), size));
  });
  region_allocator_.set_on_merge_callback([this](Address start, size_t size) {
    DCHECK(IsAligned(start, allocation_granularity()));
    CHECK(reservation_.MergePlaceholders(reinterpret_cast<void*>(start), size));
  });
#endif  // V8_OS_WIN
}

VirtualAddressSubspace::~VirtualAddressSubspace() {
  CHECK(parent_space_->FreeSubspace(this));
}

void VirtualAddressSubspace::SetRandomSeed(int64_t seed) {
  MutexGuard guard(&mutex_);
  rng_.SetSeed(seed);
}

Address VirtualAddressSubspace::RandomPageAddress() {
  MutexGuard guard(&mutex_);
  // Note: the random numbers generated here aren't uniformly distributed if the
  // size isn't a power of two.
  Address addr = base() + (rng_.NextInt64() % size());
  return RoundDown(addr, allocation_granularity());
}

Address VirtualAddressSubspace::AllocatePages(Address hint, size_t size,
                                              size_t alignment,
                                              PagePermissions permissions) {
  DCHECK(IsAligned(alignment, allocation_granularity()));
  DCHECK(IsAligned(hint, alignment));
  DCHECK(IsAligned(size, allocation_granularity()));

  MutexGuard guard(&mutex_);

  Address address = region_allocator_.AllocateRegion(hint, size, alignment);
  if (address == RegionAllocator::kAllocationFailure) return kNullAddress;

  if (!reservation_.Allocate(reinterpret_cast<void*>(address), size,
                             static_cast<OS::MemoryPermission>(permissions))) {
    // This most likely means that we ran out of memory.
    CHECK_EQ(size, region_allocator_.FreeRegion(address));
    return kNullAddress;
  }

  return address;
}

bool VirtualAddressSubspace::FreePages(Address address, size_t size) {
  DCHECK(IsAligned(address, allocation_granularity()));
  DCHECK(IsAligned(size, allocation_granularity()));

  MutexGuard guard(&mutex_);
  if (region_allocator_.CheckRegion(address) != size) return false;

  // The order here is important: on Windows, the allocation first has to be
  // freed to a placeholder before the placeholder can be merged (during the
  // merge_callback) with any surrounding placeholder mappings.
  CHECK(reservation_.Free(reinterpret_cast<void*>(address), size));
  CHECK_EQ(size, region_allocator_.FreeRegion(address));
  return true;
}

bool VirtualAddressSubspace::SetPagePermissions(Address address, size_t size,
                                                PagePermissions permissions) {
  DCHECK(IsAligned(address, page_size()));
  DCHECK(IsAligned(size, page_size()));

  return reservation_.SetPermissions(
      reinterpret_cast<void*>(address), size,
      static_cast<OS::MemoryPermission>(permissions));
}

std::unique_ptr<v8::VirtualAddressSpace>
VirtualAddressSubspace::AllocateSubspace(Address hint, size_t size,
                                         size_t alignment,
                                         PagePermissions max_permissions) {
  DCHECK(IsAligned(alignment, allocation_granularity()));
  DCHECK(IsAligned(hint, alignment));
  DCHECK(IsAligned(size, allocation_granularity()));

  MutexGuard guard(&mutex_);

  Address address = region_allocator_.AllocateRegion(hint, size, alignment);
  if (address == RegionAllocator::kAllocationFailure) {
    return std::unique_ptr<v8::VirtualAddressSpace>();
  }

  base::Optional<AddressSpaceReservation> reservation =
      reservation_.CreateSubReservation(
          reinterpret_cast<void*>(address), size,
          static_cast<OS::MemoryPermission>(max_permissions));
  if (!reservation.has_value()) {
    CHECK_EQ(size, region_allocator_.FreeRegion(address));
    return nullptr;
  }
  return std::unique_ptr<v8::VirtualAddressSpace>(
      new VirtualAddressSubspace(*reservation, this));
}

bool VirtualAddressSubspace::DiscardSystemPages(Address address, size_t size) {
  DCHECK(IsAligned(address, page_size()));
  DCHECK(IsAligned(size, page_size()));

  return reservation_.DiscardSystemPages(reinterpret_cast<void*>(address),
                                         size);
}

bool VirtualAddressSubspace::DecommitPages(Address address, size_t size) {
  DCHECK(IsAligned(address, page_size()));
  DCHECK(IsAligned(size, page_size()));

  return reservation_.DecommitPages(reinterpret_cast<void*>(address), size);
}

bool VirtualAddressSubspace::FreeSubspace(VirtualAddressSubspace* subspace) {
  MutexGuard guard(&mutex_);

  AddressSpaceReservation reservation = subspace->reservation_;
  Address base = reinterpret_cast<Address>(reservation.base());
  if (region_allocator_.FreeRegion(base) != reservation.size()) {
    return false;
  }

  return reservation_.FreeSubReservation(reservation);
}

}  // namespace base
}  // namespace v8
