// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/code-memory-access.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(ThreadIsolation, ReuseJitPage) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  ThreadIsolation::RegisterJitPage(address1, size);

  Address address2 = address1 + size;
  ThreadIsolation::RegisterJitPage(address2, size);

  ThreadIsolation::UnregisterJitPage(address1, size);
  ThreadIsolation::RegisterJitPage(address1, size);
  ThreadIsolation::UnregisterJitPage(address1, size);
  ThreadIsolation::UnregisterJitPage(address2, size);

  ThreadIsolation::RegisterJitPage(address1, 2 * size);
  ThreadIsolation::UnregisterJitPage(address1, 2 * size);
}

TEST(ThreadIsolation, CatchJitPageOverlap) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  ThreadIsolation::RegisterJitPage(address1, size);
  EXPECT_DEATH_IF_SUPPORTED(
      { ThreadIsolation::RegisterJitPage(address1 + size - 1, 1); }, "");
  ThreadIsolation::UnregisterJitPage(address1, size);
}

TEST(ThreadIsolation, JitAllocation) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  ThreadIsolation::RegisterJitPage(address1, size);

  Address address2 = address1 + size;
  ThreadIsolation::RegisterJitPage(address2, size);

  ThreadIsolation::RegisterJitAllocationForTesting(address2 + size - 1, 1);
  ThreadIsolation::RegisterJitAllocationForTesting(address1, 1);
  // An allocation spanning two pages.
  ThreadIsolation::RegisterJitAllocationForTesting(address2 - 1, 2);

  ThreadIsolation::UnregisterJitPage(address1, size);
  // The spanning allocation should've been released, try to reuse the memory.
  ThreadIsolation::RegisterJitAllocationForTesting(address2, 1);
  ThreadIsolation::UnregisterJitPage(address2, size);
}

TEST(ThreadIsolation, CatchOOBJitAllocation) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  ThreadIsolation::RegisterJitPage(address1, size);
  EXPECT_DEATH_IF_SUPPORTED(
      { ThreadIsolation::RegisterJitAllocationForTesting(address1 + size, 1); },
      "");
  ThreadIsolation::UnregisterJitPage(address1, size);
}

TEST(ThreadIsolation, MergeJitPages) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  Address address2 = address1 + size;
  Address address3 = address2 + size;

  ThreadIsolation::RegisterJitPage(address2, size);
  ThreadIsolation::RegisterJitPage(address1, size);
  ThreadIsolation::RegisterJitPage(address3, size);

  ThreadIsolation::RegisterJitAllocationForTesting(address1, 3 * size);
  ThreadIsolation::UnregisterJitAllocationForTesting(address1, 3 * size);

  // Test merge in both directions
  ThreadIsolation::UnregisterJitPage(address2, size);
  ThreadIsolation::RegisterJitPage(address2, size);

  ThreadIsolation::RegisterJitAllocationForTesting(address1, 3 * size);
  ThreadIsolation::UnregisterJitAllocationForTesting(address1, 3 * size);

  ThreadIsolation::UnregisterJitPage(address2, size);
  ThreadIsolation::UnregisterJitPage(address1, size);
  ThreadIsolation::UnregisterJitPage(address3, size);
}

TEST(ThreadIsolation, UnregisterAllocationsExcept) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  ThreadIsolation::RegisterJitPage(address1, size);

  std::vector<base::AddressRegion> allocations;
  std::vector<base::Address> to_keep;

  allocations.emplace_back(address1, 1);
  allocations.emplace_back(address1 + 1, 1);
  allocations.emplace_back(address1 + 10, 1);
  allocations.emplace_back(address1 + size - 1, 1);

  for (auto allocation : allocations) {
    ThreadIsolation::RegisterJitAllocationForTesting(allocation.begin(),
                                                     allocation.size());
  }

  to_keep.emplace_back(address1);
  to_keep.emplace_back(address1 + size - 1);

  ThreadIsolation::UnregisterJitAllocationsInPageExceptForTesting(
      address1, size, to_keep);

  // Everything should be free except first and last byte.
  ThreadIsolation::RegisterJitAllocationForTesting(address1 + 1, size - 2);

  // But we should've kept to_keep[0].
  EXPECT_DEATH_IF_SUPPORTED(
      { ThreadIsolation::RegisterJitAllocationForTesting(to_keep[0], 1); }, "");

  ThreadIsolation::UnregisterJitPage(address1, size);
}

TEST(ThreadIsolation, UnregisterAllocationsExceptNextPage) {
  ThreadIsolation::Initialize(nullptr);

  Address address1 = 0x4100000;
  size_t size = 0x1000;
  Address address2 = address1 + size;
  ThreadIsolation::RegisterJitPage(address1, size);
  ThreadIsolation::RegisterJitPage(address2, size);

  std::vector<base::AddressRegion> allocations;
  std::vector<base::Address> to_keep;

  allocations.emplace_back(address1, 1);
  allocations.emplace_back(address1 + 1, 1);
  allocations.emplace_back(address1 + 10, 1);
  allocations.emplace_back(address1 + size - 1, 1);

  allocations.emplace_back(address2, 1);

  for (auto allocation : allocations) {
    ThreadIsolation::RegisterJitAllocationForTesting(allocation.begin(),
                                                     allocation.size());
  }

  ThreadIsolation::UnregisterJitAllocationsInPageExceptForTesting(
      address1, size, to_keep);

  // Everything should be free.
  ThreadIsolation::RegisterJitAllocationForTesting(address1, size);

  // But we should've kept the allocation on the next page.
  EXPECT_DEATH_IF_SUPPORTED(
      { ThreadIsolation::RegisterJitAllocationForTesting(address2, 1); }, "");

  ThreadIsolation::UnregisterJitPage(address1, size);
  ThreadIsolation::UnregisterJitPage(address2, size);
}

}  // namespace internal
}  // namespace v8
