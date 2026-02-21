// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/incremental-marking-schedule.h"

#include "src/base/platform/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap::base {

namespace {

constexpr size_t kZeroBytesStep = 0;

// Minimum number of bytes that should be marked during an incremental
// marking step.
constexpr size_t kMinimumMarkedBytesPerIncrementalStep =
    IncrementalMarkingSchedule::kStepSizeWhenNotMakingProgress;

class IncrementalMarkingScheduleTest : public ::testing::Test {
 public:
  static constexpr size_t kEstimatedLiveSize =
      100 * kMinimumMarkedBytesPerIncrementalStep;
};

const v8::base::TimeDelta kHalfEstimatedMarkingTime =
    v8::base::TimeDelta::FromMillisecondsD(
        IncrementalMarkingSchedule::kEstimatedMarkingTime.InMillisecondsF() *
        0.5);

}  // namespace

TEST_F(IncrementalMarkingScheduleTest, FirstStepReturnsDefaultDuration) {
  auto schedule =
      IncrementalMarkingSchedule::CreateWithMarkedBytesPerStepForTesting(
          kMinimumMarkedBytesPerIncrementalStep);
  schedule->NotifyIncrementalMarkingStart();
  schedule->SetElapsedTimeForTesting(v8::base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(kMinimumMarkedBytesPerIncrementalStep,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

TEST_F(IncrementalMarkingScheduleTest, EmptyStepDuration) {
  auto schedule = IncrementalMarkingSchedule::Create();
  schedule->NotifyIncrementalMarkingStart();
  schedule->SetElapsedTimeForTesting(v8::base::TimeDelta::FromMilliseconds(0));
  // Make some progress on the marker to avoid returning step size for no
  // progress.
  schedule->AddMutatorThreadMarkedBytes(
      IncrementalMarkingSchedule::kStepSizeWhenNotMakingProgress);
  EXPECT_EQ(kZeroBytesStep,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

// If marking is not behind schedule and very small time passed between steps
// the oracle should return the minimum step duration.
TEST_F(IncrementalMarkingScheduleTest, NoTimePassedReturnsMinimumDuration) {
  auto schedule =
      IncrementalMarkingSchedule::CreateWithMarkedBytesPerStepForTesting(
          kMinimumMarkedBytesPerIncrementalStep);
  schedule->NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  schedule->AddMutatorThreadMarkedBytes(kMinimumMarkedBytesPerIncrementalStep);
  schedule->SetElapsedTimeForTesting(v8::base::TimeDelta::FromMilliseconds(0));
  EXPECT_EQ(kMinimumMarkedBytesPerIncrementalStep,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

TEST_F(IncrementalMarkingScheduleTest, OracleDoesntExccedMaximumStepDuration) {
  auto schedule =
      IncrementalMarkingSchedule::CreateWithMarkedBytesPerStepForTesting(
          kMinimumMarkedBytesPerIncrementalStep);
  schedule->NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  // Add at least `kStepSizeWhenNotMakingProgress` bytes or otherwise we'd get
  // the step size for not making progress.
  static constexpr size_t kMarkedBytes =
      IncrementalMarkingSchedule::kStepSizeWhenNotMakingProgress;
  schedule->AddMutatorThreadMarkedBytes(kMarkedBytes);
  schedule->SetElapsedTimeForTesting(
      IncrementalMarkingSchedule::kEstimatedMarkingTime);
  EXPECT_EQ(kEstimatedLiveSize - kMarkedBytes,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

TEST_F(IncrementalMarkingScheduleTest, AheadOfScheduleReturnsMinimumDuration) {
  auto schedule =
      IncrementalMarkingSchedule::CreateWithMarkedBytesPerStepForTesting(
          kMinimumMarkedBytesPerIncrementalStep);
  schedule->NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  schedule->AddMutatorThreadMarkedBytes(kMinimumMarkedBytesPerIncrementalStep);
  schedule->AddConcurrentlyMarkedBytes(0.6 * kEstimatedLiveSize);
  schedule->SetElapsedTimeForTesting(kHalfEstimatedMarkingTime);
  EXPECT_EQ(kMinimumMarkedBytesPerIncrementalStep,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

TEST_F(IncrementalMarkingScheduleTest,
       AheadOfScheduleReturnsMinimumDurationZeroStep) {
  auto schedule = IncrementalMarkingSchedule::Create();
  schedule->NotifyIncrementalMarkingStart();
  // Add incrementally marked bytes to tell oracle this is not the first step.
  schedule->AddMutatorThreadMarkedBytes(kMinimumMarkedBytesPerIncrementalStep);
  schedule->AddConcurrentlyMarkedBytes(0.6 * kEstimatedLiveSize);
  schedule->SetElapsedTimeForTesting(kHalfEstimatedMarkingTime);
  EXPECT_EQ(kZeroBytesStep,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

TEST_F(IncrementalMarkingScheduleTest, BehindScheduleReturnsDelta) {
  auto schedule =
      IncrementalMarkingSchedule::CreateWithMarkedBytesPerStepForTesting(
          kMinimumMarkedBytesPerIncrementalStep);
  schedule->NotifyIncrementalMarkingStart();
  schedule->AddMutatorThreadMarkedBytes(0.1 * kEstimatedLiveSize);
  schedule->AddConcurrentlyMarkedBytes(0.25 * kEstimatedLiveSize);
  schedule->SetElapsedTimeForTesting(kHalfEstimatedMarkingTime);
  EXPECT_EQ(0.15 * kEstimatedLiveSize,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
  schedule->AddConcurrentlyMarkedBytes(0.05 * kEstimatedLiveSize);
  schedule->SetElapsedTimeForTesting(kHalfEstimatedMarkingTime);
  EXPECT_EQ(0.1 * kEstimatedLiveSize,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
  schedule->AddConcurrentlyMarkedBytes(0.05 * kEstimatedLiveSize);
  schedule->SetElapsedTimeForTesting(kHalfEstimatedMarkingTime);
  EXPECT_EQ(0.05 * kEstimatedLiveSize,
            schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize));
}

TEST_F(IncrementalMarkingScheduleTest, GetCurrentStepInfo) {
  auto schedule =
      IncrementalMarkingSchedule::CreateWithMarkedBytesPerStepForTesting(
          kMinimumMarkedBytesPerIncrementalStep);
  schedule->NotifyIncrementalMarkingStart();
  schedule->AddMutatorThreadMarkedBytes(0.3 * kEstimatedLiveSize);
  schedule->AddConcurrentlyMarkedBytes(0.4 * kEstimatedLiveSize);
  schedule->SetElapsedTimeForTesting(kHalfEstimatedMarkingTime);
  schedule->GetNextIncrementalStepDuration(kEstimatedLiveSize);
  const auto step_info = schedule->GetCurrentStepInfo();
  EXPECT_EQ(step_info.elapsed_time, kHalfEstimatedMarkingTime);
  EXPECT_EQ(step_info.mutator_marked_bytes, 0.3 * kEstimatedLiveSize);
  EXPECT_EQ(step_info.concurrent_marked_bytes, 0.4 * kEstimatedLiveSize);
  EXPECT_EQ(step_info.marked_bytes(), 0.7 * kEstimatedLiveSize);
  EXPECT_EQ(step_info.estimated_live_bytes, kEstimatedLiveSize);
  EXPECT_NE(step_info.scheduled_delta_bytes(), 0);
}

}  // namespace heap::base
