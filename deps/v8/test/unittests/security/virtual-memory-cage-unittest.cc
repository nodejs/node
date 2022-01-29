// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/base/virtual-address-space.h"
#include "src/security/vm-cage.h"
#include "test/unittests/test-utils.h"

#ifdef V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

namespace v8 {
namespace internal {

TEST(VirtualMemoryCageTest, Initialization) {
  base::VirtualAddressSpace vas;

  V8VirtualMemoryCage cage;

  EXPECT_FALSE(cage.is_initialized());
  EXPECT_FALSE(cage.is_disabled());
  EXPECT_FALSE(cage.is_fake_cage());
  EXPECT_EQ(cage.size(), 0UL);

  EXPECT_TRUE(cage.Initialize(&vas));

  EXPECT_TRUE(cage.is_initialized());
  EXPECT_NE(cage.base(), 0UL);
  EXPECT_GT(cage.size(), 0UL);

  cage.TearDown();

  EXPECT_FALSE(cage.is_initialized());
}

TEST(VirtualMemoryCageTest, InitializationWithSize) {
  base::VirtualAddressSpace vas;
  // This test only works if virtual memory subspaces can be allocated.
  if (!vas.CanAllocateSubspaces()) return;

  V8VirtualMemoryCage cage;
  size_t size = kVirtualMemoryCageMinimumSize;
  const bool use_guard_regions = false;
  EXPECT_TRUE(cage.Initialize(&vas, size, use_guard_regions));

  EXPECT_TRUE(cage.is_initialized());
  EXPECT_FALSE(cage.is_fake_cage());
  EXPECT_EQ(cage.size(), size);

  cage.TearDown();
}

TEST(VirtualMemoryCageTest, InitializationAsFakeCage) {
  base::VirtualAddressSpace vas;
  V8VirtualMemoryCage cage;
  // Total size of the fake cage.
  size_t size = kVirtualMemoryCageSize;
  // Size of the virtual memory that is actually reserved at the start of the
  // cage.
  size_t reserved_size = 2 * vas.allocation_granularity();
  EXPECT_TRUE(cage.InitializeAsFakeCage(&vas, size, reserved_size));

  EXPECT_TRUE(cage.is_initialized());
  EXPECT_TRUE(cage.is_fake_cage());
  EXPECT_NE(cage.base(), 0UL);
  EXPECT_EQ(cage.size(), size);

  cage.TearDown();

  EXPECT_FALSE(cage.is_initialized());
}

TEST(VirtualMemloryCageTest, Contains) {
  base::VirtualAddressSpace vas;
  V8VirtualMemoryCage cage;
  EXPECT_TRUE(cage.Initialize(&vas));

  Address base = cage.base();
  size_t size = cage.size();
  base::RandomNumberGenerator rng(::testing::FLAGS_gtest_random_seed);

  EXPECT_TRUE(cage.Contains(base));
  EXPECT_TRUE(cage.Contains(base + size - 1));
  for (int i = 0; i < 10; i++) {
    size_t offset = rng.NextInt64() % size;
    EXPECT_TRUE(cage.Contains(base + offset));
  }

  EXPECT_FALSE(cage.Contains(base - 1));
  EXPECT_FALSE(cage.Contains(base + size));
  for (int i = 0; i < 10; i++) {
    Address addr = rng.NextInt64();
    if (addr < base || addr >= base + size) {
      EXPECT_FALSE(cage.Contains(addr));
    }
  }

  cage.TearDown();
}

void TestCagePageAllocation(V8VirtualMemoryCage& cage) {
  const size_t kAllocatinSizesInPages[] = {1, 1, 2, 3, 5, 8, 13, 21, 34};
  constexpr int kNumAllocations = arraysize(kAllocatinSizesInPages);

  VirtualAddressSpace* vas = cage.virtual_address_space();
  size_t allocation_granularity = vas->allocation_granularity();
  std::vector<Address> allocations;
  for (int i = 0; i < kNumAllocations; i++) {
    size_t length = allocation_granularity * kAllocatinSizesInPages[i];
    size_t alignment = allocation_granularity;
    Address ptr = vas->AllocatePages(VirtualAddressSpace::kNoHint, length,
                                     alignment, PagePermissions::kNoAccess);
    EXPECT_NE(ptr, kNullAddress);
    EXPECT_TRUE(cage.Contains(ptr));
    allocations.push_back(ptr);
  }

  for (int i = 0; i < kNumAllocations; i++) {
    size_t length = allocation_granularity * kAllocatinSizesInPages[i];
    EXPECT_TRUE(vas->FreePages(allocations[i], length));
  }
}

TEST(VirtualMemoryCageTest, PageAllocation) {
  base::VirtualAddressSpace vas;
  V8VirtualMemoryCage cage;
  EXPECT_TRUE(cage.Initialize(&vas));

  TestCagePageAllocation(cage);

  cage.TearDown();
}

TEST(VirtualMemoryCageTest, FakeCagePageAllocation) {
  base::VirtualAddressSpace vas;
  V8VirtualMemoryCage cage;
  size_t size = kVirtualMemoryCageSize;
  // Only reserve two pages so the test will allocate memory inside and outside
  // of the reserved region.
  size_t reserved_size = 2 * vas.allocation_granularity();
  EXPECT_TRUE(cage.InitializeAsFakeCage(&vas, size, reserved_size));

  TestCagePageAllocation(cage);

  cage.TearDown();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE
