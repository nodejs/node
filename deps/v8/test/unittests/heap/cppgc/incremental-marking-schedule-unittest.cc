// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/incremental-marking-schedule.h"

#include "src/base/platform/time.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/stats-collector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class IncrementalMarkingScheduleTest : public testing::Test {
 public:
  static const size_t kObjectSize;
};

const size_t IncrementalMarkingScheduleTest::kObjectSize =
    100 * IncrementalMarkingSchedule::kMinimumMarkedBytesPerIncrementalStep;

}  // namespace

TEST_F(IncrementalMarkingScheduleTest, FirstStepReturnsDefaultDuration) {
  IncrementalMarkingSchedule schedule;
  schedule.NotifyIncrementalMarkingStart();
  schedule.SetElapsedTimeForTesting(0);
  EXPECT_EQ(IncrementalMarkingSchedule::kMinimumMarkedBytesPerIncrementalStep,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
}

// If marking is not behind schedule and very small time passed between steps
// the oracle should return the minimum step duration.
TEST_F(IncrementalMarkingScheduleTest, NoTimePassedReturnsMinimumDuration) {
  IncrementalMarkingSchedule schedule;
  schedule.NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  schedule.UpdateIncrementalMarkedBytes(
      IncrementalMarkingSchedule::kMinimumMarkedBytesPerIncrementalStep);
  schedule.SetElapsedTimeForTesting(0);
  EXPECT_EQ(IncrementalMarkingSchedule::kMinimumMarkedBytesPerIncrementalStep,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
}

TEST_F(IncrementalMarkingScheduleTest, OracleDoesntExccedMaximumStepDuration) {
  IncrementalMarkingSchedule schedule;
  schedule.NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  static constexpr size_t kMarkedBytes = 1;
  schedule.UpdateIncrementalMarkedBytes(kMarkedBytes);
  schedule.SetElapsedTimeForTesting(
      IncrementalMarkingSchedule::kEstimatedMarkingTimeMs);
  EXPECT_EQ(kObjectSize - kMarkedBytes,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
}

TEST_F(IncrementalMarkingScheduleTest, AheadOfScheduleReturnsMinimumDuration) {
  IncrementalMarkingSchedule schedule;
  schedule.NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  schedule.UpdateIncrementalMarkedBytes(
      IncrementalMarkingSchedule::kMinimumMarkedBytesPerIncrementalStep);
  schedule.AddConcurrentlyMarkedBytes(0.6 * kObjectSize);
  schedule.SetElapsedTimeForTesting(
      0.5 * IncrementalMarkingSchedule::kEstimatedMarkingTimeMs);
  EXPECT_EQ(IncrementalMarkingSchedule::kMinimumMarkedBytesPerIncrementalStep,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
}

TEST_F(IncrementalMarkingScheduleTest, BehindScheduleReturnsCorrectDuration) {
  IncrementalMarkingSchedule schedule;
  schedule.NotifyIncrementalMarkingStart();
  schedule.UpdateIncrementalMarkedBytes(0.1 * kObjectSize);
  schedule.AddConcurrentlyMarkedBytes(0.25 * kObjectSize);
  schedule.SetElapsedTimeForTesting(
      0.5 * IncrementalMarkingSchedule::kEstimatedMarkingTimeMs);
  EXPECT_EQ(0.15 * kObjectSize,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
  schedule.AddConcurrentlyMarkedBytes(0.05 * kObjectSize);
  schedule.SetElapsedTimeForTesting(
      0.5 * IncrementalMarkingSchedule::kEstimatedMarkingTimeMs);
  EXPECT_EQ(0.1 * kObjectSize,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
  schedule.AddConcurrentlyMarkedBytes(0.05 * kObjectSize);
  schedule.SetElapsedTimeForTesting(
      0.5 * IncrementalMarkingSchedule::kEstimatedMarkingTimeMs);
  EXPECT_EQ(0.05 * kObjectSize,
            schedule.GetNextIncrementalStepDuration(kObjectSize));
}

}  // namespace internal
}  // namespace cppgc
