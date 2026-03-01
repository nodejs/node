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
// File: simulated_clock.h
// -----------------------------------------------------------------------------

#ifndef ABSL_TIME_SIMULATED_CLOCK_H_
#define ABSL_TIME_SIMULATED_CLOCK_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include "absl/base/config.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/clock_interface.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// A simulated clock is a concrete Clock implementation that does not "tick"
// on its own.  Time is advanced by explicit calls to the AdvanceTime() or
// SetTime() functions.
//
// Example:
//   absl::SimulatedClock sim_clock;
//   absl::Time now = sim_clock.TimeNow();
//   // now == absl::UnixEpoch()
//
//   now = sim_clock.TimeNow();
//   // now == absl::UnixEpoch() (still)
//
//   sim_clock.AdvanceTime(absl::Seconds(3));
//   now = sim_clock.TimeNow();
//   // now == absl::UnixEpoch() + absl::Seconds(3)
//
// This class is thread-safe.
class SimulatedClock : public Clock {
 public:
  explicit SimulatedClock(absl::Time t);
  SimulatedClock() : SimulatedClock(absl::UnixEpoch()) {}

  // The destructor should be called only if all Sleep(), etc. and
  // AdvanceTime() calls have completed. The code does its best to let
  // any pending calls finish gracefully, but there are no guarantees.
  ~SimulatedClock() override;

  // Returns the simulated time.
  absl::Time TimeNow() override;

  // Sleeps until the specified duration has elapsed according to this clock.
  void Sleep(absl::Duration d) override;

  // Sleeps until the specified wakeup_time.
  void SleepUntil(absl::Time wakeup_time) override;

  // Sets the simulated time to the argument.  Wakes up any threads whose
  // sleeps have now expired. Returns the number of woken threads.
  int64_t SetTime(absl::Time t);

  // Advances the simulated time by the specified duration.  Wakes up any
  // threads whose sleeps have now expired. Returns the number of woken threads.
  int64_t AdvanceTime(absl::Duration d);

  // Blocks until the condition is true or until the simulated clock is
  // advanced to or beyond the wakeup time (or both).
  bool AwaitWithDeadline(absl::Mutex* absl_nonnull mu,
                         const absl::Condition& cond,
                         absl::Time deadline) override
      ABSL_SHARED_LOCKS_REQUIRED(mu);

  // Returns the earliest wakeup time.
  std::optional<absl::Time> GetEarliestWakeupTime() const;

 private:
  template <class T>
  int64_t UpdateTime(const T& now_updater) ABSL_LOCKS_EXCLUDED(lock_);

  class WakeUpInfo;
  using WaiterList = std::multimap<absl::Time, std::shared_ptr<WakeUpInfo>>;

  mutable absl::Mutex lock_;
  absl::Time now_ ABSL_GUARDED_BY(lock_);
  WaiterList waiters_ ABSL_GUARDED_BY(lock_);
  int64_t num_await_calls_ ABSL_GUARDED_BY(lock_) = 0;
};

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TIME_SIMULATED_CLOCK_H_
