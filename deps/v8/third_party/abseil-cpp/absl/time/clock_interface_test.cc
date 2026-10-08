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

#include "absl/time/clock_interface.h"

#include <cstdint>
#include <functional>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace {

constexpr absl::Duration kLongPause = absl::Milliseconds(150);

#ifdef _MSC_VER
// As of 2026-01-29, multithreaded tests on MSVC are too flaky.
const char* kSkipFlakyReason =
    "Skipping this timing test because it is too flaky";
#else
const char* kSkipFlakyReason = nullptr;
#endif


void AwaitWithDeadlineAndNotify(absl::Clock* clock, absl::Mutex* mu,
                                absl::Condition* cond, absl::Time wakeup_time,
                                absl::Notification* note, bool* return_val) {
  mu->lock_shared();
  *return_val = clock->AwaitWithDeadline(mu, *cond, wakeup_time);
  mu->unlock_shared();
  note->Notify();
}

TEST(RealClockTest, AwaitWithVeryLargeDeadline) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::Clock& clock = absl::Clock::GetRealClock();
  absl::Mutex mu;
  bool f = false;
  absl::Condition cond(&f);
  absl::Notification note;
  bool return_val;

  std::thread thread(AwaitWithDeadlineAndNotify, &clock, &mu, &cond,
                     absl::InfiniteFuture(), &note, &return_val);

  EXPECT_FALSE(note.HasBeenNotified());

  mu.lock();
  f = true;
  mu.unlock();

  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  EXPECT_TRUE(return_val);
  note.WaitForNotification();  // In case the expectation fails.
  thread.join();
}

TEST(RealClockTest, AwaitWithPastDeadline) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::Clock& clock = absl::Clock::GetRealClock();
  absl::Mutex mu;
  bool f = false;
  absl::Condition cond(&f);
  absl::Notification note;
  bool return_val;

  std::thread thread(AwaitWithDeadlineAndNotify, &clock, &mu, &cond,
                     clock.TimeNow() - absl::Seconds(10), &note, &return_val);
  absl::Time start = absl::Now();
  note.WaitForNotification();  // AwaitWithDeadline() should ping note.
  EXPECT_GE(kLongPause, absl::Now() - start);
  EXPECT_FALSE(return_val);
  thread.join();
}

TEST(RealClockTest, AwaitWithSmallDeadline) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::Clock& clock = absl::Clock::GetRealClock();
  absl::Mutex mu;
  bool f = false;
  absl::Condition cond(&f);

  for (int i = 0; i < 5; ++i) {
    absl::Duration wait_time = absl::Milliseconds(i);
    absl::Duration elapsed_time;
    {
      absl::MutexLock lock(mu);
      absl::Time start = clock.TimeNow();
      absl::Time wakeup_time = start + wait_time;
      EXPECT_FALSE(clock.AwaitWithDeadline(&mu, cond, wakeup_time));
      elapsed_time = clock.TimeNow() - start;
    }
    if (elapsed_time >= absl::ZeroDuration()) {  // ignore jumps backwards
      // 60 microseconds of slop to allow for non-monotonic clocks.
      EXPECT_GE(elapsed_time, wait_time - absl::Microseconds(60));
    }
  }
}

}  // namespace
