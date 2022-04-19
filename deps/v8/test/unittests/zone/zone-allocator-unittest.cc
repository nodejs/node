// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone-allocator.h"

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class RecyclingZoneAllocatorTest : public TestWithPlatform {};

TEST_F(RecyclingZoneAllocatorTest, ReuseSameSize) {
  AccountingAllocator accounting_allocator;
  Zone zone(&accounting_allocator, ZONE_NAME);
  RecyclingZoneAllocator<int> zone_allocator(&zone);

  int* allocated = zone_allocator.allocate(10);
  zone_allocator.deallocate(allocated, 10);
  CHECK_EQ(zone_allocator.allocate(10), allocated);
}

TEST_F(RecyclingZoneAllocatorTest, ReuseSmallerSize) {
  AccountingAllocator accounting_allocator;
  Zone zone(&accounting_allocator, ZONE_NAME);
  RecyclingZoneAllocator<int> zone_allocator(&zone);

  int* allocated = zone_allocator.allocate(100);
  zone_allocator.deallocate(allocated, 100);
  CHECK_EQ(zone_allocator.allocate(10), allocated);
}

TEST_F(RecyclingZoneAllocatorTest, DontReuseTooSmallSize) {
  AccountingAllocator accounting_allocator;
  Zone zone(&accounting_allocator, ZONE_NAME);
  RecyclingZoneAllocator<int> zone_allocator(&zone);

  // The sizeof(FreeBlock) will be larger than a single int, so we can't keep
  // store the free list in the deallocated block.
  int* allocated = zone_allocator.allocate(1);
  zone_allocator.deallocate(allocated, 1);
  CHECK_NE(zone_allocator.allocate(1), allocated);
}

TEST_F(RecyclingZoneAllocatorTest, ReuseMultipleSize) {
  AccountingAllocator accounting_allocator;
  Zone zone(&accounting_allocator, ZONE_NAME);
  RecyclingZoneAllocator<int> zone_allocator(&zone);

  int* allocated1 = zone_allocator.allocate(10);
  int* allocated2 = zone_allocator.allocate(20);
  int* allocated3 = zone_allocator.allocate(30);
  zone_allocator.deallocate(allocated1, 10);
  zone_allocator.deallocate(allocated2, 20);
  zone_allocator.deallocate(allocated3, 30);
  CHECK_EQ(zone_allocator.allocate(10), allocated3);
  CHECK_EQ(zone_allocator.allocate(10), allocated2);
  CHECK_EQ(zone_allocator.allocate(10), allocated1);
}

TEST_F(RecyclingZoneAllocatorTest, DontChainSmallerSizes) {
  AccountingAllocator accounting_allocator;
  Zone zone(&accounting_allocator, ZONE_NAME);
  RecyclingZoneAllocator<int> zone_allocator(&zone);

  int* allocated1 = zone_allocator.allocate(10);
  int* allocated2 = zone_allocator.allocate(5);
  int* allocated3 = zone_allocator.allocate(10);
  zone_allocator.deallocate(allocated1, 10);
  zone_allocator.deallocate(allocated2, 5);
  zone_allocator.deallocate(allocated3, 10);
  CHECK_EQ(zone_allocator.allocate(5), allocated3);
  CHECK_EQ(zone_allocator.allocate(5), allocated1);
  CHECK_NE(zone_allocator.allocate(5), allocated2);
}

}  // namespace internal
}  // namespace v8
