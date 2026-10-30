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
//
// -----------------------------------------------------------------------------
// File: mock_clock.h
// -----------------------------------------------------------------------------

#ifndef ABSL_TIME_MOCK_CLOCK_H_
#define ABSL_TIME_MOCK_CLOCK_H_

#include "gmock/gmock.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock_interface.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// A gMock implementation of absl::Clock.
//
// This class is appropriate for testing single-threaded uses of
// absl::Clock, particularly when calling
// absl::Clock::Sleep().  absl::MockClock::Sleep() (by default) simply
// advances simulated time and returns.
//
// For multithreaded testing, consider using absl::SimulatedClock.
class MockClock : public absl::Clock {
 public:
  explicit MockClock(absl::Time now) : now_(now) {
    ON_CALL(*this, TimeNow())
        .WillByDefault(testing::Invoke(this, &MockClock::current_time));
    ON_CALL(*this, Sleep(testing::A<absl::Duration>()))
        .WillByDefault(testing::Invoke(this, &MockClock::SleepTime));
    ON_CALL(*this, SleepUntil(testing::A<absl::Time>()))
        .WillByDefault(testing::Invoke(this, &MockClock::SetTime));
  }

  MockClock() : MockClock(absl::UnixEpoch()) {}

  MOCK_METHOD(absl::Time, TimeNow, (), (override));
  MOCK_METHOD(void, Sleep, (absl::Duration d), (override));
  MOCK_METHOD(void, SleepUntil, (absl::Time wakeup_time), (override));
  MOCK_METHOD(bool, AwaitWithDeadline,
              (absl::Mutex* mu, const absl::Condition& cond,
               absl::Time wakeup_time),
              (override));

  absl::Time current_time() const { return now_; }

  // Set the mock time to 'timestamp'.
  void SetTime(absl::Time timestamp) { now_ = timestamp; }

  // Advance the mock time and return immediately.
  void SleepTime(absl::Duration d) { now_ += d; }

 private:
  absl::Time now_;  // The mock time.
};

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TIME_MOCK_CLOCK_H_
