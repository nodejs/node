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

#include "absl/time/simulated_clock.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/thread_annotations.h"
#include "absl/random/random.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/clock_interface.h"
#include "absl/time/time.h"

namespace {

constexpr absl::Duration kShortPause = absl::Milliseconds(20);
constexpr absl::Duration kLongPause = absl::Milliseconds(1000);

#ifdef _MSC_VER
// As of 2026-01-29, multithreaded tests on MSVC are too flaky.
const char* kSkipFlakyReason =
    "Skipping this timing test because it is too flaky";
#else
const char* kSkipFlakyReason = nullptr;
#endif

TEST(SimulatedClock, TimeInitializedToZero) {
  absl::SimulatedClock simclock;
  EXPECT_EQ(absl::UnixEpoch(), simclock.TimeNow());
}

TEST(SimulatedClock, NowSetTime) {
  absl::SimulatedClock simclock;
  absl::Time now = simclock.TimeNow();

  now += absl::Seconds(123);
  simclock.SetTime(now);
  EXPECT_EQ(now, simclock.TimeNow());

  now += absl::Seconds(123);
  simclock.SetTime(now);
  EXPECT_EQ(now, simclock.TimeNow());

  now += absl::ZeroDuration();
  simclock.SetTime(now);
  EXPECT_EQ(now, simclock.TimeNow());
}

TEST(SimulatedClock, NowAdvanceTime) {
  absl::SimulatedClock simclock;
  absl::Time now = simclock.TimeNow();

  simclock.AdvanceTime(absl::Seconds(123));
  now += absl::Seconds(123);
  EXPECT_EQ(now, simclock.TimeNow());

  simclock.AdvanceTime(absl::Seconds(123));
  now += absl::Seconds(123);
  EXPECT_EQ(now, simclock.TimeNow());

  simclock.AdvanceTime(absl::ZeroDuration());
  now += absl::ZeroDuration();
  EXPECT_EQ(now, simclock.TimeNow());
}

void SleepAndNotify(absl::Clock* clock, absl::Duration sleep_secs,
                    absl::Notification* note) {
  clock->Sleep(sleep_secs);
  note->Notify();
}

TEST(SimulatedClock, Sleep_SetToSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  std::thread tr(SleepAndNotify, &simclock, absl::Seconds(123), &note);

  // wait for SleepAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.SetTime(absl::FromUnixSeconds(122));
  // give Sleep() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.SetTime(absl::FromUnixSeconds(122 + 1));
  // wait for Sleep() to return
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, SleepAdvanceToSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  std::thread tr(SleepAndNotify, &simclock, absl::Seconds(123), &note);
  // wait for SleepAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.AdvanceTime(absl::Seconds(122));
  // give Sleep() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.AdvanceTime(absl::Seconds(1));
  // wait for Sleep() to return
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, SleepSetPastSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  std::thread tr(SleepAndNotify, &simclock, absl::Seconds(123), &note);
  // wait for SleepAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.SetTime(absl::FromUnixSeconds(122));
  // give Sleep() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.SetTime(absl::FromUnixSeconds(122 + 2));
  // wait for Sleep() to return
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, SleepAdvancePastSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  std::thread tr(SleepAndNotify, &simclock, absl::Seconds(123), &note);
  // wait for SleepAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.AdvanceTime(absl::Seconds(122));
  // give Sleep() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.AdvanceTime(absl::Seconds(2));
  // wait for Sleep() to return
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, SleepZeroSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  std::thread tr(SleepAndNotify, &simclock, absl::ZeroDuration(), &note);
  // wait for SleepAndNotify() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

void SleepUntilAndNotify(absl::Clock* clock, absl::Time wakeup_time,
                         absl::Notification* note) {
  clock->SleepUntil(wakeup_time);
  note->Notify();
}

TEST(SimulatedClock, SleepUntilSetToSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  simclock.SetTime(absl::FromUnixSeconds(123));
  std::thread tr(SleepUntilAndNotify, &simclock,
                 absl::UnixEpoch() + absl::Seconds(246), &note);
  // wait for SleepUntilAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.SetTime(absl::FromUnixSeconds(123 + 122));
  // give SleepUntil() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.SetTime(absl::FromUnixSeconds(123 + 122 + 1));
  absl::Time start = absl::Now();
  note.WaitForNotification();  // SleepUntilAndNotify() should ping note
  EXPECT_GE(absl::Milliseconds(50), absl::Now() - start);
  tr.join();
}

TEST(SimulatedClock, SleepUntilAdvanceToSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  simclock.AdvanceTime(absl::Seconds(123));
  std::thread tr(SleepUntilAndNotify, &simclock,
                 absl::UnixEpoch() + absl::Seconds(246), &note);
  // wait for SleepUntilAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.AdvanceTime(absl::Seconds(122));
  // give SleepUntil() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.AdvanceTime(absl::Seconds(1));
  absl::Time start = absl::Now();
  note.WaitForNotification();  // SleepUntilAndNotify() should ping note
  EXPECT_GE(absl::Milliseconds(70), absl::Now() - start);
  tr.join();
}

TEST(SimulatedClock, SleepUntilSetPastSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  simclock.SetTime(absl::FromUnixSeconds(123));
  std::thread tr(SleepUntilAndNotify, &simclock,
                 absl::UnixEpoch() + absl::Seconds(246), &note);
  // wait for SleepUntilAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.SetTime(absl::FromUnixSeconds(123 + 122));
  // give SleepUntil() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.SetTime(absl::FromUnixSeconds(123 + 122 + 2));
  // wait for SleepUntilAndNotify() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, SleepUntilAdvancePastSleepTime) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  simclock.AdvanceTime(absl::Seconds(123));
  std::thread tr(SleepUntilAndNotify, &simclock,
                 absl::UnixEpoch() + absl::Seconds(246), &note);
  // wait for SleepUntilAndNotify() to block
  absl::SleepFor(kLongPause);
  simclock.AdvanceTime(absl::Seconds(122));
  // give SleepUntil() the opportunity to fail
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.AdvanceTime(absl::Seconds(2));
  // wait for SleepUntilAndNotify() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, SleepUntilTimeAlreadyPassed) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Notification note;

  simclock.AdvanceTime(absl::Seconds(123));
  std::thread tr(SleepUntilAndNotify, &simclock,
                 absl::UnixEpoch() + absl::Seconds(123), &note);
  // wait for SleepUntilAndNotify() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

void AwaitWithDeadlineAndNotify(absl::Clock* clock, absl::Mutex* mu,
                                absl::Condition* cond, absl::Time wakeup_time,
                                absl::Notification* note, bool* return_val) {
  mu->lock_shared();
  *return_val = clock->AwaitWithDeadline(mu, *cond, wakeup_time);
  mu->unlock_shared();
  note->Notify();
}

TEST(SimulatedClock, AwaitWithDeadlineConditionInitiallyTrue) {
  absl::SimulatedClock simclock;
  absl::Mutex mu;
  bool f = true;
  absl::Condition cond(&f);
  absl::MutexLock lock(mu);
  ASSERT_TRUE(simclock.AwaitWithDeadline(&mu, cond, absl::InfiniteFuture()));
}

TEST(SimulatedClock, AwaitWithDeadlineConditionInitiallyFalse) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Mutex mu;
  bool f = false;
  absl::Condition cond(&f);
  absl::Notification note;
  bool return_val;

  std::thread tr(AwaitWithDeadlineAndNotify, &simclock, &mu, &cond,
                 absl::UnixEpoch() + absl::Seconds(123), &note, &return_val);
  // wait for AwaitWithDeadline...() to block
  absl::SleepFor(kShortPause);
  EXPECT_FALSE(note.HasBeenNotified());
  mu.lock();
  f = true;
  mu.unlock();
  // wait for AwaitWithDeadline...() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  EXPECT_TRUE(return_val);
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, AwaitWithDeadlineDeadlinePassed) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Mutex mu;
  bool f = false;
  absl::Condition cond(&f);
  absl::Notification note;
  bool return_val;

  std::thread tr(AwaitWithDeadlineAndNotify, &simclock, &mu, &cond,
                 absl::UnixEpoch() + absl::Seconds(123), &note, &return_val);
  // wait for AwaitWithDeadline...() to block
  absl::SleepFor(kLongPause);
  EXPECT_FALSE(note.HasBeenNotified());
  simclock.AdvanceTime(absl::Seconds(124));
  // wait for AwaitWithDeadline...() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  EXPECT_FALSE(return_val);
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

TEST(SimulatedClock, AwaitWithDeadlineDeadlineAlreadyPassed) {
  if (kSkipFlakyReason != nullptr) {
    GTEST_SKIP() << kSkipFlakyReason;
  }

  absl::SimulatedClock simclock;
  absl::Mutex mu;
  bool f = false;
  absl::Condition cond(&f);
  absl::Notification note;
  bool return_val;

  std::thread tr(AwaitWithDeadlineAndNotify, &simclock, &mu, &cond,
                 absl::UnixEpoch(), &note, &return_val);
  // wait for AwaitWithDeadline...() to ping note
  absl::SleepFor(kLongPause);
  EXPECT_TRUE(note.HasBeenNotified());
  EXPECT_FALSE(return_val);
  note.WaitForNotification();  // in case the expectation fails
  tr.join();
}

void RacerMakesConditionTrue(absl::Notification* start_note, absl::Mutex* mu,
                             bool* f, absl::BlockingCounter* threads_done) {
  start_note->WaitForNotification();
  absl::SleepFor(absl::Milliseconds(1));
  mu->lock();
  *f = true;
  mu->unlock();
  threads_done->DecrementCount();
}

void RacerAdvancesTime(absl::Notification* start_note,
                       absl::SimulatedClock* simclock, absl::Duration d,
                       absl::BlockingCounter* threads_done) {
  start_note->WaitForNotification();
  absl::SleepFor(absl::Milliseconds(1));
  simclock->AdvanceTime(d);
  threads_done->DecrementCount();
}

TEST(SimulatedClock, SimultaneousConditionTrueAndDeadline) {
  absl::SimulatedClock simclock;
  for (int iteration = 0; iteration < 100; ++iteration) {
    auto mu = std::make_unique<absl::Mutex>();
    bool f = false;
    absl::Condition cond(&f);
    absl::Notification note_start;
    absl::BlockingCounter threads_done(2);
    std::thread tr1(RacerMakesConditionTrue, &note_start, mu.get(), &f,
                    &threads_done);
    std::thread tr2(RacerAdvancesTime, &note_start, &simclock,
                    absl::Seconds(20), &threads_done);
    note_start.Notify();
    mu->lock();
    absl::Time deadline = simclock.TimeNow() + absl::Seconds(10);
    simclock.AwaitWithDeadline(mu.get(), cond, deadline);
    EXPECT_TRUE(f || (simclock.TimeNow() >= deadline));
    if (f) {
      // RacerMakesConditionTrue has unlocked mu and AwaitWithDeadline has
      // returned, so it is safe to destruct mu. Do so while RacerAdvancesTime
      // is possibly still running in an attempt to catch simclock holding on
      // to a reference to mu and using it after AwaitWithDeadline returns.
      mu->unlock();
      mu = nullptr;
    } else {
      mu->unlock();
    }
    threads_done.Wait();
    tr1.join();
    tr2.join();
  }
}

void RacerDeletesClock(absl::Mutex* mu, absl::Notification* start_note,
                       absl::Clock* clock,
                       absl::BlockingCounter* threads_done) {
  start_note->WaitForNotification();
  // mu is acquired temporarily to make sure that AwaitWithDeadline() in
  // SimultaneousConditionTrueAndDestruction has blocked.
  mu->lock();
  mu->unlock();
  absl::SleepFor(absl::Milliseconds(1));
  delete clock;
  threads_done->DecrementCount();
}

TEST(SimulatedClock, SimultaneousConditionTrueAndDestruction) {
  for (int iteration = 0; iteration < 100; ++iteration) {
    absl::Clock* clock = new absl::SimulatedClock();
    absl::Mutex mu;
    bool f = false;
    absl::Condition cond(&f);
    absl::Notification note_start;
    absl::BlockingCounter threads_done(2);
    std::thread tr1([&note_start, &mu, &f, &threads_done] {
      RacerMakesConditionTrue(&note_start, &mu, &f, &threads_done);
    });
    std::thread tr2([&mu, &note_start, clock, &threads_done] {
      RacerDeletesClock(&mu, &note_start, clock, &threads_done);
    });
    mu.lock();
    note_start.Notify();
    absl::Time deadline = absl::UnixEpoch() + absl::Seconds(100000);
    clock->AwaitWithDeadline(&mu, cond, deadline);
    mu.unlock();
    threads_done.Wait();
    tr1.join();
    tr2.join();
  }
}

class SimulatedClockTorturer {
 public:
  SimulatedClockTorturer(absl::SimulatedClock* simclock, int num_threads,
                         int num_iterations)
      : simclock_(simclock),
        num_threads_(num_threads),
        num_iterations_(num_iterations),
        num_flags_(2 * num_threads),
        mutex_and_flag_(static_cast<size_t>(num_flags_)) {}

  // Implements a torture test.
  //
  // This method uses several groups of:
  //   SimulatedClock
  //   Mutex protected flag
  // It starts several threads that call AwaitWithDeadline() and several
  // threads that call AdvanceTime() or toggle flag values.
  void DoTorture() {
    // The threads calling AwaitWithDeadline() have a separate BlockingCounter
    // than the threads calling AdvanceTime()/toggling flags, since the former
    // would be deadlocked if all the threads that might unblock them had
    // already finished.
    absl::Notification go;
    absl::BlockingCounter await_threads_done(num_threads_);
    absl::Notification signal_threads_should_exit;
    absl::BlockingCounter signal_threads_done(num_threads_);
    std::vector<std::thread> trs;
    for (int i = 0; i < num_threads_; ++i) {
      trs.emplace_back(&SimulatedClockTorturer::AwaitRandomly, this, &go,
                       &await_threads_done);
    }
    for (int i = 0; i < num_threads_; ++i) {
      trs.emplace_back(&SimulatedClockTorturer::SignalRandomly, this, &go,
                       &signal_threads_should_exit, &signal_threads_done);
    }
    go.Notify();
    await_threads_done.Wait();
    signal_threads_should_exit.Notify();
    signal_threads_done.Wait();
    for (auto& thread : trs) {
      thread.join();
    }
  }

 private:
  // Randomly call AwaitWithDeadline() for num_iterations_ times.
  void AwaitRandomly(absl::Notification* go,
                     absl::BlockingCounter* threads_done) {
    go->WaitForNotification();

    absl::BitGen gen;
    for (int i = 0; i < num_iterations_; ++i) {
      auto& [mu, f] = mutex_and_flag_[absl::Uniform<size_t>(gen, size_t{0},
                                         static_cast<size_t>(num_flags_))];
      absl::MutexLock lock(mu);
      absl::Time deadline = simclock_->TimeNow() + absl::Seconds(1);
      simclock_->AwaitWithDeadline(&mu, absl::Condition(&f), deadline);
      ABSL_RAW_CHECK(f || simclock_->TimeNow() >= deadline, "");
    }

    threads_done->DecrementCount();
  }

  // Randomly call AdvanceTime() or toggle a flag value until notified to
  // stop.
  void SignalRandomly(absl::Notification* go, absl::Notification* should_exit,
                      absl::BlockingCounter* threads_done) {
    go->WaitForNotification();

    absl::BitGen gen;
    while (!should_exit->HasBeenNotified()) {
      int action = absl::Uniform<int>(gen, 0, num_flags_ + 1);
      if (action < num_flags_) {
        // Change a flag value.
        auto& [mutex, flag] = mutex_and_flag_[static_cast<size_t>(action)];
        absl::MutexLock lock(mutex);
        flag = !flag;
      } else {
        // Advance time.
        simclock_->AdvanceTime(absl::Seconds(1));
      }
    }

    threads_done->DecrementCount();
  }

  absl::SimulatedClock* simclock_;
  int num_threads_;
  int num_iterations_;
  int num_flags_;

  struct MutexAndFlag {
    absl::Mutex mutex;
    bool flag ABSL_GUARDED_BY(mutex) = false;
  };
  std::vector<MutexAndFlag> mutex_and_flag_;
};

TEST(SimulatedClock, Torture) {
  absl::SimulatedClock simclock;
  constexpr int kNumThreads = 10;
  constexpr int kNumIterations = 1000;
  SimulatedClockTorturer torturer(&simclock, kNumThreads, kNumIterations);
  torturer.DoTorture();
}

}  // namespace
