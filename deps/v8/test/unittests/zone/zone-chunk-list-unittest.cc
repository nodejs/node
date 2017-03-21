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

}  // namespace internal
}  // namespace v8
