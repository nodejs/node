// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/globals.h"
#include "src/heap/scavenge-job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

const size_t kScavengeSpeedInBytesPerMs = 500 * KB;
const size_t kNewSpaceCapacity = 8 * MB;


TEST(ScavengeJob, AllocationLimitEmptyNewSpace) {
  EXPECT_FALSE(ScavengeJob::ReachedIdleAllocationLimit(
      kScavengeSpeedInBytesPerMs, 0, kNewSpaceCapacity));
}


TEST(ScavengeJob, AllocationLimitFullNewSpace) {
  EXPECT_TRUE(ScavengeJob::ReachedIdleAllocationLimit(
      kScavengeSpeedInBytesPerMs, kNewSpaceCapacity, kNewSpaceCapacity));
}


TEST(ScavengeJob, AllocationLimitUnknownScavengeSpeed) {
  size_t expected_size = ScavengeJob::kInitialScavengeSpeedInBytesPerMs *
                             ScavengeJob::kAverageIdleTimeMs -
                         ScavengeJob::kBytesAllocatedBeforeNextIdleTask;
  EXPECT_FALSE(ScavengeJob::ReachedIdleAllocationLimit(0, expected_size - 1,
                                                       kNewSpaceCapacity));
  EXPECT_TRUE(ScavengeJob::ReachedIdleAllocationLimit(0, expected_size,
                                                      kNewSpaceCapacity));
}


TEST(ScavengeJob, AllocationLimitLowScavengeSpeed) {
  size_t scavenge_speed = 1 * KB;
  EXPECT_FALSE(ScavengeJob::ReachedIdleAllocationLimit(
      scavenge_speed, ScavengeJob::kMinAllocationLimit - 1, kNewSpaceCapacity));
  EXPECT_TRUE(ScavengeJob::ReachedIdleAllocationLimit(
      scavenge_speed, ScavengeJob::kMinAllocationLimit, kNewSpaceCapacity));
}


TEST(ScavengeJob, AllocationLimitAverageScavengeSpeed) {
  size_t expected_size =
      kScavengeSpeedInBytesPerMs * ScavengeJob::kAverageIdleTimeMs -
      ScavengeJob::kBytesAllocatedBeforeNextIdleTask;
  EXPECT_FALSE(ScavengeJob::ReachedIdleAllocationLimit(
      kScavengeSpeedInBytesPerMs, ScavengeJob::kMinAllocationLimit,
      kNewSpaceCapacity));
  EXPECT_FALSE(ScavengeJob::ReachedIdleAllocationLimit(
      kScavengeSpeedInBytesPerMs, expected_size - 1, kNewSpaceCapacity));
  EXPECT_TRUE(ScavengeJob::ReachedIdleAllocationLimit(
      kScavengeSpeedInBytesPerMs, expected_size, kNewSpaceCapacity));
}


TEST(ScavengeJob, AllocationLimitHighScavengeSpeed) {
  size_t scavenge_speed = kNewSpaceCapacity;
  size_t expected_size =
      static_cast<size_t>(
          kNewSpaceCapacity *
          ScavengeJob::kMaxAllocationLimitAsFractionOfNewSpace) -
      ScavengeJob::kBytesAllocatedBeforeNextIdleTask;
  EXPECT_FALSE(ScavengeJob::ReachedIdleAllocationLimit(
      scavenge_speed, expected_size - 1, kNewSpaceCapacity));
  EXPECT_TRUE(ScavengeJob::ReachedIdleAllocationLimit(
      scavenge_speed, expected_size + 1, kNewSpaceCapacity));
}


TEST(ScavengeJob, EnoughIdleTimeForScavengeUnknownScavengeSpeed) {
  size_t scavenge_speed = ScavengeJob::kInitialScavengeSpeedInBytesPerMs;
  size_t new_space_size = 1 * MB;
  size_t expected_time = (new_space_size + scavenge_speed - 1) / scavenge_speed;
  EXPECT_TRUE(
      ScavengeJob::EnoughIdleTimeForScavenge(expected_time, 0, new_space_size));
  EXPECT_FALSE(ScavengeJob::EnoughIdleTimeForScavenge(expected_time - 1, 0,
                                                      new_space_size));
}


TEST(ScavengeJob, EnoughIdleTimeForScavengeLowScavengeSpeed) {
  size_t scavenge_speed = 1 * KB;
  size_t new_space_size = 1 * MB;
  size_t expected_time = (new_space_size + scavenge_speed - 1) / scavenge_speed;
  EXPECT_TRUE(ScavengeJob::EnoughIdleTimeForScavenge(
      expected_time, scavenge_speed, new_space_size));
  EXPECT_FALSE(ScavengeJob::EnoughIdleTimeForScavenge(
      expected_time - 1, scavenge_speed, new_space_size));
}


TEST(ScavengeJob, EnoughIdleTimeForScavengeHighScavengeSpeed) {
  size_t scavenge_speed = kNewSpaceCapacity;
  size_t new_space_size = 1 * MB;
  size_t expected_time = (new_space_size + scavenge_speed - 1) / scavenge_speed;
  EXPECT_TRUE(ScavengeJob::EnoughIdleTimeForScavenge(
      expected_time, scavenge_speed, new_space_size));
  EXPECT_FALSE(ScavengeJob::EnoughIdleTimeForScavenge(
      expected_time - 1, scavenge_speed, new_space_size));
}

}  // namespace internal
}  // namespace v8
