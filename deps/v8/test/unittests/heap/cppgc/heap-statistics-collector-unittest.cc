// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-statistics-collector.h"

#include "include/cppgc/heap-statistics.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

class HeapStatisticsCollectorTest : public testing::TestWithHeap {};

TEST_F(HeapStatisticsCollectorTest, EmptyHeapBriefStatisitcs) {
  HeapStatistics brief_stats = Heap::From(GetHeap())->CollectStatistics(
      HeapStatistics::DetailLevel::kBrief);
  EXPECT_EQ(HeapStatistics::DetailLevel::kBrief, brief_stats.detail_level);
  EXPECT_EQ(0u, brief_stats.used_size_bytes);
  EXPECT_EQ(0u, brief_stats.used_size_bytes);
  EXPECT_TRUE(brief_stats.space_stats.empty());
}

TEST_F(HeapStatisticsCollectorTest, EmptyHeapDetailedStatisitcs) {
  HeapStatistics detailed_stats = Heap::From(GetHeap())->CollectStatistics(
      HeapStatistics::DetailLevel::kDetailed);
  EXPECT_EQ(HeapStatistics::DetailLevel::kDetailed,
            detailed_stats.detail_level);
  EXPECT_EQ(0u, detailed_stats.used_size_bytes);
  EXPECT_EQ(0u, detailed_stats.used_size_bytes);
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces, detailed_stats.space_stats.size());
  for (HeapStatistics::SpaceStatistics& space_stats :
       detailed_stats.space_stats) {
    EXPECT_EQ(0u, space_stats.used_size_bytes);
    EXPECT_EQ(0u, space_stats.used_size_bytes);
    EXPECT_TRUE(space_stats.page_stats.empty());
    if (space_stats.name == "LargePageSpace") {
      // Large page space has no free list.
      EXPECT_TRUE(space_stats.free_list_stats.bucket_size.empty());
      EXPECT_TRUE(space_stats.free_list_stats.free_count.empty());
      EXPECT_TRUE(space_stats.free_list_stats.free_size.empty());
    } else {
      EXPECT_EQ(kPageSizeLog2, space_stats.free_list_stats.bucket_size.size());
      EXPECT_EQ(kPageSizeLog2, space_stats.free_list_stats.free_count.size());
      EXPECT_EQ(kPageSizeLog2, space_stats.free_list_stats.free_size.size());
    }
  }
}

namespace {
template <size_t Size>
class GCed : public GarbageCollected<GCed<Size>> {
 public:
  void Trace(Visitor*) const {}

 private:
  char array_[Size];
};
}  // namespace

TEST_F(HeapStatisticsCollectorTest, NonEmptyNormalPage) {
  MakeGarbageCollected<GCed<1>>(GetHeap()->GetAllocationHandle());
  static constexpr size_t used_size =
      RoundUp<kAllocationGranularity>(1 + sizeof(HeapObjectHeader));
  HeapStatistics detailed_stats = Heap::From(GetHeap())->CollectStatistics(
      HeapStatistics::DetailLevel::kDetailed);
  EXPECT_EQ(HeapStatistics::DetailLevel::kDetailed,
            detailed_stats.detail_level);
  EXPECT_EQ(kPageSize, detailed_stats.physical_size_bytes);
  EXPECT_EQ(used_size, detailed_stats.used_size_bytes);
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces, detailed_stats.space_stats.size());
  bool found_non_empty_space = false;
  for (const HeapStatistics::SpaceStatistics& space_stats :
       detailed_stats.space_stats) {
    if (space_stats.page_stats.empty()) {
      EXPECT_EQ(0u, space_stats.physical_size_bytes);
      EXPECT_EQ(0u, space_stats.used_size_bytes);
      continue;
    }
    EXPECT_NE("LargePageSpace", space_stats.name);
    EXPECT_FALSE(found_non_empty_space);
    found_non_empty_space = true;
    EXPECT_EQ(kPageSize, space_stats.physical_size_bytes);
    EXPECT_EQ(used_size, space_stats.used_size_bytes);
    EXPECT_EQ(1u, space_stats.page_stats.size());
    EXPECT_EQ(kPageSize, space_stats.page_stats.back().physical_size_bytes);
    EXPECT_EQ(used_size, space_stats.page_stats.back().used_size_bytes);
  }
  EXPECT_TRUE(found_non_empty_space);
}

TEST_F(HeapStatisticsCollectorTest, NonEmptyLargePage) {
  MakeGarbageCollected<GCed<kLargeObjectSizeThreshold>>(
      GetHeap()->GetAllocationHandle());
  static constexpr size_t used_size = RoundUp<kAllocationGranularity>(
      kLargeObjectSizeThreshold + sizeof(HeapObjectHeader));
  static constexpr size_t physical_size =
      RoundUp<kAllocationGranularity>(used_size + sizeof(LargePage));
  HeapStatistics detailed_stats = Heap::From(GetHeap())->CollectStatistics(
      HeapStatistics::DetailLevel::kDetailed);
  EXPECT_EQ(HeapStatistics::DetailLevel::kDetailed,
            detailed_stats.detail_level);
  EXPECT_EQ(physical_size, detailed_stats.physical_size_bytes);
  EXPECT_EQ(used_size, detailed_stats.used_size_bytes);
  EXPECT_EQ(RawHeap::kNumberOfRegularSpaces, detailed_stats.space_stats.size());
  bool found_non_empty_space = false;
  for (const HeapStatistics::SpaceStatistics& space_stats :
       detailed_stats.space_stats) {
    if (space_stats.page_stats.empty()) {
      EXPECT_EQ(0u, space_stats.physical_size_bytes);
      EXPECT_EQ(0u, space_stats.used_size_bytes);
      continue;
    }
    EXPECT_EQ("LargePageSpace", space_stats.name);
    EXPECT_FALSE(found_non_empty_space);
    found_non_empty_space = true;
    EXPECT_EQ(physical_size, space_stats.physical_size_bytes);
    EXPECT_EQ(used_size, space_stats.used_size_bytes);
    EXPECT_EQ(1u, space_stats.page_stats.size());
    EXPECT_EQ(physical_size, space_stats.page_stats.back().physical_size_bytes);
    EXPECT_EQ(used_size, space_stats.page_stats.back().used_size_bytes);
  }
  EXPECT_TRUE(found_non_empty_space);
}

}  // namespace internal
}  // namespace cppgc
