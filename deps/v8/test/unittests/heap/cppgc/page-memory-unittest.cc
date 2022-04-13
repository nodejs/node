// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/page-memory.h"

#include "src/base/page-allocator.h"
#include "src/heap/cppgc/platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

TEST(MemoryRegionTest, Construct) {
  constexpr size_t kSize = 17;
  uint8_t dummy[kSize];
  const MemoryRegion region(dummy, kSize);
  EXPECT_EQ(dummy, region.base());
  EXPECT_EQ(kSize, region.size());
  EXPECT_EQ(dummy + kSize, region.end());
}

namespace {

Address AtOffset(uint8_t* base, intptr_t offset) {
  return reinterpret_cast<Address>(reinterpret_cast<intptr_t>(base) + offset);
}

}  // namespace

TEST(MemoryRegionTest, ContainsAddress) {
  constexpr size_t kSize = 7;
  uint8_t dummy[kSize];
  const MemoryRegion region(dummy, kSize);
  EXPECT_FALSE(region.Contains(AtOffset(dummy, -1)));
  EXPECT_TRUE(region.Contains(dummy));
  EXPECT_TRUE(region.Contains(dummy + kSize - 1));
  EXPECT_FALSE(region.Contains(AtOffset(dummy, kSize)));
}

TEST(MemoryRegionTest, ContainsMemoryRegion) {
  constexpr size_t kSize = 7;
  uint8_t dummy[kSize];
  const MemoryRegion region(dummy, kSize);
  const MemoryRegion contained_region1(dummy, kSize - 1);
  EXPECT_TRUE(region.Contains(contained_region1));
  const MemoryRegion contained_region2(dummy + 1, kSize - 1);
  EXPECT_TRUE(region.Contains(contained_region2));
  const MemoryRegion not_contained_region1(AtOffset(dummy, -1), kSize);
  EXPECT_FALSE(region.Contains(not_contained_region1));
  const MemoryRegion not_contained_region2(AtOffset(dummy, kSize), 1);
  EXPECT_FALSE(region.Contains(not_contained_region2));
}

TEST(PageMemoryTest, Construct) {
  constexpr size_t kOverallSize = 17;
  uint8_t dummy[kOverallSize];
  const MemoryRegion overall_region(dummy, kOverallSize);
  const MemoryRegion writeable_region(dummy + 1, kOverallSize - 2);
  const PageMemory page_memory(overall_region, writeable_region);
  EXPECT_EQ(dummy, page_memory.overall_region().base());
  EXPECT_EQ(dummy + kOverallSize, page_memory.overall_region().end());
  EXPECT_EQ(dummy + 1, page_memory.writeable_region().base());
  EXPECT_EQ(dummy + kOverallSize - 1, page_memory.writeable_region().end());
}

#if DEBUG

TEST(PageMemoryDeathTest, ConstructNonContainedRegions) {
  constexpr size_t kOverallSize = 17;
  uint8_t dummy[kOverallSize];
  const MemoryRegion overall_region(dummy, kOverallSize);
  const MemoryRegion writeable_region(dummy + 1, kOverallSize);
  EXPECT_DEATH_IF_SUPPORTED(PageMemory(overall_region, writeable_region), "");
}

#endif  // DEBUG

TEST(PageMemoryRegionTest, NormalPageMemoryRegion) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  pmr->UnprotectForTesting();
  MemoryRegion prev_overall;
  for (size_t i = 0; i < NormalPageMemoryRegion::kNumPageRegions; ++i) {
    const PageMemory pm = pmr->GetPageMemory(i);
    // Previous PageMemory aligns with the current one.
    if (prev_overall.base()) {
      EXPECT_EQ(prev_overall.end(), pm.overall_region().base());
    }
    prev_overall =
        MemoryRegion(pm.overall_region().base(), pm.overall_region().size());
    // Writeable region is contained in overall region.
    EXPECT_TRUE(pm.overall_region().Contains(pm.writeable_region()));
    EXPECT_EQ(0u, pm.writeable_region().base()[0]);
    EXPECT_EQ(0u, pm.writeable_region().end()[-1]);
    // Front guard page.
    EXPECT_EQ(pm.writeable_region().base(),
              pm.overall_region().base() + kGuardPageSize);
    // Back guard page.
    EXPECT_EQ(pm.overall_region().end(),
              pm.writeable_region().end() + kGuardPageSize);
  }
}

TEST(PageMemoryRegionTest, LargePageMemoryRegion) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr =
      std::make_unique<LargePageMemoryRegion>(allocator, oom_handler, 1024);
  pmr->UnprotectForTesting();
  const PageMemory pm = pmr->GetPageMemory();
  EXPECT_LE(1024u, pm.writeable_region().size());
  EXPECT_EQ(0u, pm.writeable_region().base()[0]);
  EXPECT_EQ(0u, pm.writeable_region().end()[-1]);
}

// See the comment in globals.h when setting |kGuardPageSize| for details.
#if !(defined(V8_TARGET_ARCH_ARM64) && defined(V8_OS_MACOS))
TEST(PageMemoryRegionTest, PlatformUsesGuardPages) {
  // This tests that the testing allocator actually uses protected guard
  // regions.
  v8::base::PageAllocator allocator;
#if defined(V8_HOST_ARCH_PPC64) && !defined(_AIX)
  EXPECT_FALSE(SupportsCommittingGuardPages(allocator));
#elif defined(V8_HOST_ARCH_ARM64) || defined(V8_HOST_ARCH_LOONG64)
  if (allocator.CommitPageSize() == 4096) {
    EXPECT_TRUE(SupportsCommittingGuardPages(allocator));
  } else {
    // Arm64 supports both 16k and 64k OS pages.
    EXPECT_FALSE(SupportsCommittingGuardPages(allocator));
  }
#else  // Regular case.
  EXPECT_TRUE(SupportsCommittingGuardPages(allocator));
#endif
}
#endif  // !(defined(V8_TARGET_ARCH_ARM64) && defined(V8_OS_MACOS))

namespace {

V8_NOINLINE uint8_t access(volatile const uint8_t& u) { return u; }

}  // namespace

TEST(PageMemoryRegionDeathTest, ReservationIsFreed) {
  // Full sequence as part of the death test macro as otherwise, the macro
  // may expand to statements that re-purpose the previously freed memory
  // and thus not crash.
  EXPECT_DEATH_IF_SUPPORTED(
      v8::base::PageAllocator allocator; FatalOutOfMemoryHandler oom_handler;
      Address base; {
        auto pmr = std::make_unique<LargePageMemoryRegion>(allocator,
                                                           oom_handler, 1024);
        base = pmr->reserved_region().base();
      } access(base[0]);
      , "");
}

TEST(PageMemoryRegionDeathTest, FrontGuardPageAccessCrashes) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  if (SupportsCommittingGuardPages(allocator)) {
    EXPECT_DEATH_IF_SUPPORTED(
        access(pmr->GetPageMemory(0).overall_region().base()[0]), "");
  }
}

TEST(PageMemoryRegionDeathTest, BackGuardPageAccessCrashes) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  if (SupportsCommittingGuardPages(allocator)) {
    EXPECT_DEATH_IF_SUPPORTED(
        access(pmr->GetPageMemory(0).writeable_region().end()[0]), "");
  }
}

TEST(PageMemoryRegionTreeTest, AddNormalLookupRemove) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  PageMemoryRegionTree tree;
  tree.Add(pmr.get());
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().end() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end()));
  tree.Remove(pmr.get());
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end() - 1));
}

TEST(PageMemoryRegionTreeTest, AddLargeLookupRemove) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  constexpr size_t kLargeSize = 5012;
  auto pmr = std::make_unique<LargePageMemoryRegion>(allocator, oom_handler,
                                                     kLargeSize);
  PageMemoryRegionTree tree;
  tree.Add(pmr.get());
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().end() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end()));
  tree.Remove(pmr.get());
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end() - 1));
}

TEST(PageMemoryRegionTreeTest, AddLookupRemoveMultiple) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr1 = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  constexpr size_t kLargeSize = 3127;
  auto pmr2 = std::make_unique<LargePageMemoryRegion>(allocator, oom_handler,
                                                      kLargeSize);
  PageMemoryRegionTree tree;
  tree.Add(pmr1.get());
  tree.Add(pmr2.get());
  ASSERT_EQ(pmr1.get(), tree.Lookup(pmr1->reserved_region().base()));
  ASSERT_EQ(pmr1.get(), tree.Lookup(pmr1->reserved_region().end() - 1));
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().base()));
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().end() - 1));
  tree.Remove(pmr1.get());
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().base()));
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().end() - 1));
  tree.Remove(pmr2.get());
  ASSERT_EQ(nullptr, tree.Lookup(pmr2->reserved_region().base()));
  ASSERT_EQ(nullptr, tree.Lookup(pmr2->reserved_region().end() - 1));
}

TEST(NormalPageMemoryPool, ConstructorEmpty) {
  v8::base::PageAllocator allocator;
  NormalPageMemoryPool pool;
  constexpr size_t kBucket = 0;
  EXPECT_EQ(NormalPageMemoryPool::Result(nullptr, nullptr), pool.Take(kBucket));
}

TEST(NormalPageMemoryPool, AddTakeSameBucket) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  const PageMemory pm = pmr->GetPageMemory(0);
  NormalPageMemoryPool pool;
  constexpr size_t kBucket = 0;
  pool.Add(kBucket, pmr.get(), pm.writeable_region().base());
  EXPECT_EQ(
      NormalPageMemoryPool::Result(pmr.get(), pm.writeable_region().base()),
      pool.Take(kBucket));
}

TEST(NormalPageMemoryPool, AddTakeNotFoundDifferentBucket) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator, oom_handler);
  const PageMemory pm = pmr->GetPageMemory(0);
  NormalPageMemoryPool pool;
  constexpr size_t kFirstBucket = 0;
  constexpr size_t kSecondBucket = 1;
  pool.Add(kFirstBucket, pmr.get(), pm.writeable_region().base());
  EXPECT_EQ(NormalPageMemoryPool::Result(nullptr, nullptr),
            pool.Take(kSecondBucket));
  EXPECT_EQ(
      NormalPageMemoryPool::Result(pmr.get(), pm.writeable_region().base()),
      pool.Take(kFirstBucket));
}

TEST(PageBackendTest, AllocateNormalUsesPool) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  PageBackend backend(allocator, oom_handler);
  constexpr size_t kBucket = 0;
  Address writeable_base1 = backend.AllocateNormalPageMemory(kBucket);
  EXPECT_NE(nullptr, writeable_base1);
  backend.FreeNormalPageMemory(kBucket, writeable_base1);
  Address writeable_base2 = backend.AllocateNormalPageMemory(kBucket);
  EXPECT_NE(nullptr, writeable_base2);
  EXPECT_EQ(writeable_base1, writeable_base2);
}

TEST(PageBackendTest, AllocateLarge) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  PageBackend backend(allocator, oom_handler);
  Address writeable_base1 = backend.AllocateLargePageMemory(13731);
  EXPECT_NE(nullptr, writeable_base1);
  Address writeable_base2 = backend.AllocateLargePageMemory(9478);
  EXPECT_NE(nullptr, writeable_base2);
  EXPECT_NE(writeable_base1, writeable_base2);
  backend.FreeLargePageMemory(writeable_base1);
  backend.FreeLargePageMemory(writeable_base2);
}

TEST(PageBackendTest, LookupNormal) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  PageBackend backend(allocator, oom_handler);
  constexpr size_t kBucket = 0;
  Address writeable_base = backend.AllocateNormalPageMemory(kBucket);
  if (kGuardPageSize) {
    EXPECT_EQ(nullptr, backend.Lookup(writeable_base - kGuardPageSize));
  }
  EXPECT_EQ(nullptr, backend.Lookup(writeable_base - 1));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base + kPageSize -
                                           2 * kGuardPageSize - 1));
  EXPECT_EQ(nullptr,
            backend.Lookup(writeable_base + kPageSize - 2 * kGuardPageSize));
  if (kGuardPageSize) {
    EXPECT_EQ(nullptr,
              backend.Lookup(writeable_base - kGuardPageSize + kPageSize - 1));
  }
}

TEST(PageBackendTest, LookupLarge) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  PageBackend backend(allocator, oom_handler);
  constexpr size_t kSize = 7934;
  Address writeable_base = backend.AllocateLargePageMemory(kSize);
  if (kGuardPageSize) {
    EXPECT_EQ(nullptr, backend.Lookup(writeable_base - kGuardPageSize));
  }
  EXPECT_EQ(nullptr, backend.Lookup(writeable_base - 1));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base + kSize - 1));
}

TEST(PageBackendDeathTest, DestructingBackendDestroysPageMemory) {
  v8::base::PageAllocator allocator;
  FatalOutOfMemoryHandler oom_handler;
  Address base;
  {
    PageBackend backend(allocator, oom_handler);
    constexpr size_t kBucket = 0;
    base = backend.AllocateNormalPageMemory(kBucket);
  }
  EXPECT_DEATH_IF_SUPPORTED(access(base[0]), "");
}

}  // namespace internal
}  // namespace cppgc
