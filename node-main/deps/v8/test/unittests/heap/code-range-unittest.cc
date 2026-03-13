// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/code-range.h"

#include "src/base/bounded-page-allocator.h"
#include "src/heap/page-metadata.h"
#include "src/utils/allocation.h"
#include "test/unittests/test-utils.h"

namespace v8::internal {

using RedZoneTest = TestWithIsolate;

constexpr size_t kPageSize = PageMetadata::kPageSize;
constexpr size_t kNumPagesInReservation = 10;
constexpr size_t kReservationSize = kNumPagesInReservation * kPageSize;

Address AllocatePage(v8::PageAllocator& allocator) {
  return reinterpret_cast<Address>(allocator.AllocatePages(
      nullptr, kPageSize, kPageSize, PageAllocator::Permission::kNoAccess));
}

bool FreePage(v8::PageAllocator& allocator, Address page) {
  return allocator.FreePages(reinterpret_cast<void*>(page), kPageSize);
}

std::pair<VirtualMemory, std::unique_ptr<base::BoundedPageAllocator>>
InitializeReservationAndAllocator() {
  v8::PageAllocator* platform_allocator =
      v8::internal::GetPlatformPageAllocator();
  VirtualMemory reservation(platform_allocator, kReservationSize,
                            v8::PageAllocator::AllocationHint(), kPageSize,
                            PageAllocator::Permission::kReadWrite);
  std::unique_ptr<base::BoundedPageAllocator> bpa =
      std::make_unique<base::BoundedPageAllocator>(
          platform_allocator, reservation.address(), kReservationSize,
          kPageSize, base::PageInitializationMode::kRecommitOnly,
          base::PageFreeingMode::kDiscard);
  return std::make_pair(std::move(reservation), std::move(bpa));
}

TEST_F(RedZoneTest, Uninitialized) {
  RedZones red_zones;
  EXPECT_FALSE(red_zones.TryAdd(base::AddressRegion(kNullAddress, 0)));
  EXPECT_FALSE(red_zones.TryRemove(base::AddressRegion(kNullAddress, 0)));
}

TEST_F(RedZoneTest, AddWholeReservationAsRedZone) {
  auto [reservation, allocator] = InitializeReservationAndAllocator();

  RedZones red_zones;
  red_zones.Initialize(allocator.get());

  // Nothing added to the red zone yet, so we should be able to allocate memory.
  Address page = AllocatePage(*allocator);
  EXPECT_NE(kNullAddress, page);
  // Free the memory to return into initial state.
  EXPECT_TRUE(FreePage(*allocator, page));

  // Add the whole region as red zone.
  EXPECT_TRUE(red_zones.TryAdd(
      base::AddressRegion(allocator->begin(), allocator->size())));
  // Allocation should not return anything anymore.
  EXPECT_EQ(kNullAddress, AllocatePage(*allocator));
  red_zones.Reset();
  // Even after resetting the red zones we won't be able to reuse the red zone
  // space.
  EXPECT_EQ(kNullAddress, AllocatePage(*allocator));
}

TEST_F(RedZoneTest, SplitOnce) {
  auto [reservation, allocator] = InitializeReservationAndAllocator();

  RedZones red_zones;
  red_zones.Initialize(allocator.get());

  // Add the whole region as red zone.
  EXPECT_TRUE(red_zones.TryAdd(
      base::AddressRegion(allocator->begin(), allocator->size())));
  // Allocation should not return anything anymore.
  EXPECT_EQ(kNullAddress, AllocatePage(*allocator));
  // We split the red zone and remove the 2nd page.
  EXPECT_TRUE(red_zones.TryRemove(
      base::AddressRegion(allocator->begin() + kPageSize, kPageSize)));
  // Subsequent allocation should return the 2nd page.
  EXPECT_EQ(allocator->begin() + kPageSize, AllocatePage(*allocator));
  // Subsequent allocation should not find anything anymore.
  EXPECT_EQ(kNullAddress, AllocatePage(*allocator));
}

TEST_F(RedZoneTest, SplitAll) {
  auto [reservation, allocator] = InitializeReservationAndAllocator();

  RedZones red_zones;
  red_zones.Initialize(allocator.get());

  // Add the whole region as red zone.
  EXPECT_TRUE(red_zones.TryAdd(
      base::AddressRegion(allocator->begin(), allocator->size())));
  // Allocation should not return anything anymore.
  EXPECT_EQ(kNullAddress, AllocatePage(*allocator));
  for (size_t i = 0; i < kNumPagesInReservation; i++) {
    EXPECT_TRUE(red_zones.TryRemove(
        base::AddressRegion(allocator->begin() + kPageSize * i, kPageSize)));
  }
  // Whole reservation should be usable again.
  for (size_t i = 0; i < kNumPagesInReservation; i++) {
    EXPECT_NE(kNullAddress, AllocatePage(*allocator));
  }
}

}  // namespace v8::internal
