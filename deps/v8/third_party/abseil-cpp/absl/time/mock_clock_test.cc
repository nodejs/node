// Copyright 2026 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/time/mock_clock.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace {

using ::testing::A;

TEST(MockClock, SatisfiesExpectations) {
  absl::MockClock mock_clock;
  const absl::Duration kDuration = absl::Seconds(100);
  const absl::Time kWakeupTime = absl::UnixEpoch() + kDuration;
  EXPECT_CALL(mock_clock, TimeNow()).Times(2);
  EXPECT_CALL(mock_clock, Sleep(A<absl::Duration>()));
  EXPECT_CALL(mock_clock, SleepUntil(A<absl::Time>()));

  EXPECT_EQ(absl::UnixEpoch(), mock_clock.TimeNow());
  mock_clock.Sleep(kDuration);
  EXPECT_EQ(kWakeupTime, mock_clock.TimeNow());
  mock_clock.SleepUntil(kWakeupTime);

  absl::MockClock mock_clock_init(absl::UnixEpoch() + kDuration);
  EXPECT_EQ(mock_clock_init.current_time(), absl::UnixEpoch() + kDuration);
}

}  // namespace
