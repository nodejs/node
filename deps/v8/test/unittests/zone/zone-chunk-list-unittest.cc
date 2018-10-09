// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone-chunk-list.h"

#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

const size_t kItemCount = size_t(1) << 10;

TEST(ZoneChunkList, ForwardIterationTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  size_t count = 0;

  for (uintptr_t item : zone_chunk_list) {
    EXPECT_EQ(static_cast<size_t>(item), count);
    count++;
  }

  EXPECT_EQ(count, kItemCount);
}

TEST(ZoneChunkList, ReverseIterationTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  size_t count = 0;

  for (auto it = zone_chunk_list.rbegin(); it != zone_chunk_list.rend(); ++it) {
    EXPECT_EQ(static_cast<size_t>(*it), kItemCount - count - 1);
    count++;
  }

  EXPECT_EQ(count, kItemCount);
}

TEST(ZoneChunkList, PushFrontTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_front(static_cast<uintptr_t>(i));
  }

  size_t count = 0;

  for (uintptr_t item : zone_chunk_list) {
    EXPECT_EQ(static_cast<size_t>(item), kItemCount - count - 1);
    count++;
  }

  EXPECT_EQ(count, kItemCount);
}

TEST(ZoneChunkList, RewindTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  zone_chunk_list.Rewind(42);

  size_t count = 0;

  for (uintptr_t item : zone_chunk_list) {
    EXPECT_EQ(static_cast<size_t>(item), count);
    count++;
  }

  EXPECT_EQ(count, 42u);
  EXPECT_EQ(count, zone_chunk_list.size());

  zone_chunk_list.Rewind(0);

  count = 0;

  for (uintptr_t item : zone_chunk_list) {
    USE(item);
    count++;
  }

  EXPECT_EQ(count, 0u);
  EXPECT_EQ(count, zone_chunk_list.size());

  zone_chunk_list.Rewind(100);

  count = 0;

  for (uintptr_t item : zone_chunk_list) {
    EXPECT_EQ(static_cast<size_t>(item), count);
    count++;
  }

  EXPECT_EQ(count, 0u);
  EXPECT_EQ(count, zone_chunk_list.size());
}

TEST(ZoneChunkList, FindTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  const size_t index = kItemCount / 2 + 42;

  EXPECT_EQ(*zone_chunk_list.Find(index), static_cast<uintptr_t>(index));

  *zone_chunk_list.Find(index) = 42;

  EXPECT_EQ(*zone_chunk_list.Find(index), 42u);
}

TEST(ZoneChunkList, CopyToTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  uintptr_t* array = zone.NewArray<uintptr_t>(kItemCount);

  zone_chunk_list.CopyTo(array);

  for (size_t i = 0; i < kItemCount; ++i) {
    EXPECT_EQ(array[i], static_cast<uintptr_t>(i));
  }
}

TEST(ZoneChunkList, SmallCopyToTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uint8_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uint8_t>(i & 0xFF));
  }

  uint8_t* array = zone.NewArray<uint8_t>(kItemCount);

  zone_chunk_list.CopyTo(array);

  for (size_t i = 0; i < kItemCount; ++i) {
    EXPECT_EQ(array[i], static_cast<uint8_t>(i & 0xFF));
  }
}

struct Fubar {
  size_t a_;
  size_t b_;
};

TEST(ZoneChunkList, BigCopyToTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<Fubar> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back({i, i + 5});
  }

  Fubar* array = zone.NewArray<Fubar>(kItemCount);

  zone_chunk_list.CopyTo(array);

  for (size_t i = 0; i < kItemCount; ++i) {
    EXPECT_EQ(array[i].a_, i);
    EXPECT_EQ(array[i].b_, i + 5);
  }
}

void TestForwardIterationOfConstList(
    const ZoneChunkList<uintptr_t>& zone_chunk_list) {
  size_t count = 0;

  for (uintptr_t item : zone_chunk_list) {
    EXPECT_EQ(static_cast<size_t>(item), count);
    count++;
  }

  EXPECT_EQ(count, kItemCount);
}

TEST(ZoneChunkList, ConstForwardIterationTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  TestForwardIterationOfConstList(zone_chunk_list);
}

TEST(ZoneChunkList, RewindAndIterate) {
  // Regression test for https://bugs.chromium.org/p/v8/issues/detail?id=7478
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<int> zone_chunk_list(&zone);

  // Fill the list enough so that it will contain 2 chunks.
  int chunk_size = static_cast<int>(ZoneChunkList<int>::StartMode::kSmall);
  for (int i = 0; i < chunk_size + 1; ++i) {
    zone_chunk_list.push_back(i);
  }

  // Rewind and fill the first chunk again.
  zone_chunk_list.Rewind();
  for (int i = 0; i < chunk_size; ++i) {
    zone_chunk_list.push_back(i);
  }

  std::vector<int> expected;
  for (int i = 0; i < chunk_size; ++i) {
    expected.push_back(i);
  }
  std::vector<int> got;

  // Iterate. This used to not yield the expected result, since the end iterator
  // was in a weird state, and the running iterator didn't reach it after the
  // first chunk.
  auto it = zone_chunk_list.begin();
  while (it != zone_chunk_list.end()) {
    int value = *it;
    got.push_back(value);
    ++it;
  }
  CHECK_EQ(expected.size(), got.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK_EQ(expected[i], got[i]);
  }
}

TEST(ZoneChunkList, PushBackPopBackSize) {
  // Regression test for https://bugs.chromium.org/p/v8/issues/detail?id=7489
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<int> zone_chunk_list(&zone);
  CHECK_EQ(size_t(0), zone_chunk_list.size());
  zone_chunk_list.push_back(1);
  CHECK_EQ(size_t(1), zone_chunk_list.size());
  zone_chunk_list.pop_back();
  CHECK_EQ(size_t(0), zone_chunk_list.size());
}

TEST(ZoneChunkList, AdvanceZeroTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  auto iterator_advance = zone_chunk_list.begin();

  iterator_advance.Advance(0);

  CHECK_EQ(iterator_advance, zone_chunk_list.begin());
}

TEST(ZoneChunkList, AdvancePartwayTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  auto iterator_advance = zone_chunk_list.begin();
  auto iterator_one_by_one = zone_chunk_list.begin();

  iterator_advance.Advance(kItemCount / 2);
  for (size_t i = 0; i < kItemCount / 2; ++i) {
    ++iterator_one_by_one;
  }

  CHECK_EQ(iterator_advance, iterator_one_by_one);
}

TEST(ZoneChunkList, AdvanceEndTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<uintptr_t> zone_chunk_list(&zone);

  for (size_t i = 0; i < kItemCount; ++i) {
    zone_chunk_list.push_back(static_cast<uintptr_t>(i));
  }

  auto iterator_advance = zone_chunk_list.begin();

  iterator_advance.Advance(kItemCount);

  CHECK_EQ(iterator_advance, zone_chunk_list.end());
}

TEST(ZoneChunkList, FindOverChunkBoundary) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneChunkList<int> zone_chunk_list(&zone);

  // Make sure we get two chunks.
  int chunk_size = static_cast<int>(ZoneChunkList<int>::StartMode::kSmall);
  for (int i = 0; i < chunk_size + 1; ++i) {
    zone_chunk_list.push_back(i);
  }

  for (int i = 0; i < chunk_size + 1; ++i) {
    CHECK_EQ(i, *zone_chunk_list.Find(i));
  }
}

}  // namespace internal
}  // namespace v8
