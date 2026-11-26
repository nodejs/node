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

// Exercises the SpinLock contention profiling hooks.  The hooks can only be
// registered once per process, so this test lives in its own binary and must
// not be linked together with another profiler.

#include <atomic>
#include <cstdint>
#include <thread>  // NOLINT(build/c++11)

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/cycleclock.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/spinlock.h"
#include "absl/synchronization/notification.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

struct SpinLockProfilingTestPeer {
  // True once a waiter has marked itself as about to sleep in the lock word.
  static bool HasSleeper(const SpinLock& l) {
    return (l.lockword_.load(std::memory_order_relaxed) &
            SpinLock::kWaitTimeMask) != 0;
  }
};

namespace {

const void* profiled_lock = nullptr;
std::atomic<int64_t> lock_profiler_calls{0};
std::atomic<int64_t> lock_profiler_cycles{0};
std::atomic<int64_t> unlock_profiler_calls{0};
std::atomic<int64_t> unlock_profiler_cycles{0};

void LockProfiler(const void* lock, int64_t wait_cycles) {
  if (lock != profiled_lock) return;
  lock_profiler_calls.fetch_add(1, std::memory_order_relaxed);
  lock_profiler_cycles.fetch_add(wait_cycles, std::memory_order_relaxed);
}

void UnlockProfiler(const void* lock, int64_t wait_cycles) {
  if (lock != profiled_lock) return;
  unlock_profiler_calls.fetch_add(1, std::memory_order_relaxed);
  unlock_profiler_cycles.fetch_add(wait_cycles, std::memory_order_relaxed);
}

class SpinLockProfiling : public ::testing::Test {
 protected:
  void SetUp() override {
    RegisterSpinLockProfiler(&UnlockProfiler);
    RegisterSpinLockLockProfiler(&LockProfiler);
    lock_profiler_calls.store(0, std::memory_order_relaxed);
    lock_profiler_cycles.store(0, std::memory_order_relaxed);
    unlock_profiler_calls.store(0, std::memory_order_relaxed);
    unlock_profiler_cycles.store(0, std::memory_order_relaxed);
  }

  void TearDown() override { profiled_lock = nullptr; }
};

TEST_F(SpinLockProfiling, ContentionProfilers) {
  SpinLock spinlock(SCHEDULE_KERNEL_ONLY);
  profiled_lock = &spinlock;

  // Hold the lock until a waiter is about to sleep on it, then keep holding
  // it for kContendedHoldCycles so that both the waiter's wait and the
  // holder's contended hold are comfortably above the reporting threshold.
  constexpr int64_t kContendedHoldCycles = int64_t{1} << 20;
  constexpr int64_t kWaiterHoldCycles = int64_t{1} << 12;
  Notification held;
  std::thread holder([&] {
    spinlock.lock();
    held.Notify();
    while (!SpinLockProfilingTestPeer::HasSleeper(spinlock)) {
      std::this_thread::yield();
    }
    const int64_t end = CycleClock::Now() + kContendedHoldCycles;
    while (CycleClock::Now() < end) {
      // Keep holding the lock while the waiter sleeps on it.
    }
    spinlock.unlock();
  });
  held.WaitForNotification();
  // Causes contention, since `holder` already holds the lock.
  spinlock.lock();
  const int64_t end = CycleClock::Now() + kWaiterHoldCycles;
  while (CycleClock::Now() < end) {
    // Hold the lock long enough for this hold to be reported as well.
  }
  spinlock.unlock();
  holder.join();
  profiled_lock = nullptr;

  // The waiter's acquisition reported its own wait.
  EXPECT_EQ(1, lock_profiler_calls.load(std::memory_order_relaxed));
  EXPECT_GE(lock_profiler_cycles.load(std::memory_order_relaxed),
            kContendedHoldCycles);
  // The holder's release reported the contended portion of its hold.  The
  // waiter slept, so it cannot know whether other sleepers remain and its own
  // hold is also reported as contended.
  EXPECT_EQ(2, unlock_profiler_calls.load(std::memory_order_relaxed));
  EXPECT_GE(unlock_profiler_cycles.load(std::memory_order_relaxed),
            kContendedHoldCycles);
}

TEST_F(SpinLockProfiling, UncontendedAcquisitionsAreNotReported) {
  SpinLock spinlock(SCHEDULE_KERNEL_ONLY);
  profiled_lock = &spinlock;
  for (int i = 0; i < 1000; ++i) {
    spinlock.lock();
    spinlock.unlock();
  }
  profiled_lock = nullptr;

  EXPECT_EQ(0, lock_profiler_calls.load(std::memory_order_relaxed));
  EXPECT_EQ(0, unlock_profiler_calls.load(std::memory_order_relaxed));
}

}  // namespace
}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
