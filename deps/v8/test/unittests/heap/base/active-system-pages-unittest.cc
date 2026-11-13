// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/active-system-pages.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace heap {
namespace base {

TEST(ActiveSystemPagesTest, Add) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(pages.Add(0, 1, kPageSizeBits), size_t{1});
  EXPECT_EQ(pages.Add(1, 2, kPageSizeBits), size_t{1});
  EXPECT_EQ(pages.Add(63, 64, kPageSizeBits), size_t{1});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{3});

  // Try to add page a second time.
  EXPECT_EQ(pages.Add(0, 2, kPageSizeBits), size_t{0});
}

TEST(ActiveSystemPagesTest, AddUnalignedRange) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 12;
  const size_t kPageSize = size_t{1} << kPageSizeBits;
  const size_t kWordSize = 8;
  EXPECT_EQ(pages.Add(0, kPageSize + kWordSize, kPageSizeBits), size_t{2});
  EXPECT_EQ(pages.Add(3 * kPageSize - kWordSize, 3 * kPageSize, kPageSizeBits),
            size_t{1});
  EXPECT_EQ(pages.Add(kPageSize + kWordSize, 3 * kPageSize - kWordSize,
                      kPageSizeBits),
            size_t{0});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{3} * kPageSize);
}

TEST(ActiveSystemPagesTest, AddFullBitset) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(pages.Add(0, 64, kPageSizeBits), size_t{64});
  EXPECT_EQ(pages.Add(0, 64, kPageSizeBits), size_t{0});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{64});
}

TEST(ActiveSystemPagesTest, Remove) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(pages.Add(0, 4, kPageSizeBits), size_t{4});
  EXPECT_EQ(pages.Remove(1, 3, kPageSizeBits), size_t{2});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{2});

  // Removing already removed pages should return 0.
  EXPECT_EQ(pages.Remove(1, 3, kPageSizeBits), size_t{0});
  // Removing overlapping range should only count active pages.
  EXPECT_EQ(pages.Remove(0, 4, kPageSizeBits), size_t{2});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{0});
}

TEST(ActiveSystemPagesTest, RemoveUnalignedRange) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 12;
  const size_t kPageSize = size_t{1} << kPageSizeBits;
  const size_t kWordSize = 8;
  EXPECT_EQ(pages.Add(0, 4 * kPageSize, kPageSizeBits), size_t{4});

  // Range smaller than a page or straddling page boundary without covering a
  // full page should remove 0 pages.
  EXPECT_EQ(pages.Remove(kWordSize, kPageSize - kWordSize, kPageSizeBits),
            size_t{0});
  EXPECT_EQ(
      pages.Remove(kPageSize - kWordSize, kPageSize + kWordSize, kPageSizeBits),
      size_t{0});

  // Range [kWordSize, 3 * kPageSize - kWordSize) only fully covers page 1.
  EXPECT_EQ(pages.Remove(kWordSize, 3 * kPageSize - kWordSize, kPageSizeBits),
            size_t{1});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{3} * kPageSize);
}

TEST(ActiveSystemPagesTest, RemoveFullBitset) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(pages.Add(0, 64, kPageSizeBits), size_t{64});
  EXPECT_EQ(pages.Remove(0, 64, kPageSizeBits), size_t{64});
  EXPECT_EQ(pages.Remove(0, 64, kPageSizeBits), size_t{0});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{0});
}

TEST(ActiveSystemPagesTest, Reduce) {
  ActiveSystemPages original;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(original.Add(0, 3, kPageSizeBits), size_t{3});

  ActiveSystemPages updated;
  EXPECT_EQ(updated.Add(1, 3, kPageSizeBits), size_t{2});

  EXPECT_EQ(original.Reduce(updated), size_t{1});
}

TEST(ActiveSystemPagesTest, ReduceFullBitset) {
  ActiveSystemPages original;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(original.Add(0, 64, kPageSizeBits), size_t{64});

  ActiveSystemPages updated;
  EXPECT_EQ(updated.Add(63, 64, kPageSizeBits), size_t{1});

  EXPECT_EQ(original.Reduce(updated), size_t{63});
}

TEST(ActiveSystemPagesTest, Clear) {
  ActiveSystemPages pages;
  const size_t kPageSizeBits = 0;
  EXPECT_EQ(pages.Add(0, 64, kPageSizeBits), size_t{64});
  EXPECT_EQ(pages.Clear(), size_t{64});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{0});

  EXPECT_EQ(pages.Add(0, 2, kPageSizeBits), size_t{2});
  EXPECT_EQ(pages.Clear(), size_t{2});
  EXPECT_EQ(pages.Size(kPageSizeBits), size_t{0});
}

}  // namespace base
}  // namespace heap
