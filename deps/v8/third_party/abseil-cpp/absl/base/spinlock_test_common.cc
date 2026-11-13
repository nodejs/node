// Copyright 2017 The Abseil Authors.
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

// A bunch of threads repeatedly hash an array of ints protected by a
// spinlock.  If the spinlock is working properly, all elements of the
// array should be equal at the end of the test.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>  // NOLINT(build/c++11)
#include <random>
#include <thread>  // NOLINT(build/c++11)
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/low_level_scheduling.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/spinlock.h"
#include "absl/base/internal/sysinfo.h"
#include "absl/base/macros.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"

constexpr uint32_t kNumThreads = 10;
constexpr int32_t kIters = 1000;

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

// This is defined outside of anonymous namespace so that it can be
// a friend of SpinLock to access protected methods for testing.
struct SpinLockTest {
  static uint32_t EncodeWaitStart(int64_t now) {
    return SpinLock::EncodeWaitStart(now);
  }
  static int64_t DecodeWaitCycles(uint32_t lock_value, int64_t now) {
    return SpinLock::DecodeWaitCycles(lock_value, now);
  }

  static bool IsCooperative(const SpinLock& l) { return l.IsCooperative(); }
};

namespace {

static constexpr size_t kArrayLength = 10;
static uint32_t values[kArrayLength];

ABSL_CONST_INIT static SpinLock static_cooperative_spinlock;
ABSL_CONST_INIT static SpinLock static_noncooperative_spinlock(
    base_internal::SCHEDULE_KERNEL_ONLY);

// Simple integer hash function based on the public domain lookup2 hash.
// http://burtleburtle.net/bob/c/lookup2.c
static uint32_t Hash32(uint32_t a, uint32_t c) {
  uint32_t b = 0x9e3779b9UL;  // The golden ratio; an arbitrary value.
  a -= b;
  a -= c;
  a ^= (c >> 13);
  b -= c;
  b -= a;
  b ^= (a << 8);
  c -= a;
  c -= b;
  c ^= (b >> 13);
  a -= b;
  a -= c;
  a ^= (c >> 12);
  b -= c;
  b -= a;
  b ^= (a << 16);
  c -= a;
  c -= b;
  c ^= (b >> 5);
  a -= b;
  a -= c;
  a ^= (c >> 3);
  b -= c;
  b -= a;
  b ^= (a << 10);
  c -= a;
  c -= b;
  c ^= (b >> 15);
  return c;
}

static void TestFunction(uint32_t thread_salt, SpinLock* spinlock) {
  for (int i = 0; i < kIters; i++) {
    SpinLockHolder h(*spinlock);
    for (size_t j = 0; j < kArrayLength; j++) {
      const size_t index = (j + thread_salt) % kArrayLength;
      values[index] = Hash32(values[index], thread_salt);
      std::this_thread::yield();
    }
  }
}

static void ThreadedTest(SpinLock* spinlock) {
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (uint32_t i = 0; i < kNumThreads; ++i) {
    threads.push_back(std::thread(TestFunction, i, spinlock));
  }
  for (auto& thread : threads) {
    thread.join();
  }

  SpinLockHolder h(*spinlock);
  for (size_t i = 1; i < kArrayLength; i++) {
    EXPECT_EQ(values[0], values[i]);
  }
}

#ifndef ABSL_HAVE_THREAD_SANITIZER
static_assert(std::is_trivially_destructible<SpinLock>());
#endif

TEST(SpinLock, StackNonCooperativeDisablesScheduling) {
  SpinLock spinlock(base_internal::SCHEDULE_KERNEL_ONLY);
  SpinLockHolder l(spinlock);
  EXPECT_FALSE(base_internal::SchedulingGuard::ReschedulingIsAllowed());
}

TEST(SpinLock, StaticNonCooperativeDisablesScheduling) {
  SpinLockHolder l(static_noncooperative_spinlock);
  EXPECT_FALSE(base_internal::SchedulingGuard::ReschedulingIsAllowed());
}

TEST(SpinLock, WaitStartEncoding) {
  // These are implementation details not exported by SpinLock.
  const int kProfileTimestampShift = 7;
  const int kLockwordReservedShift = 3;

  // The encoded timestamp wraps around at 2^kWrapShift cycles, and the lower
  // kProfileTimestampShift bits are dropped.
  const int kWrapShift = 32 - kLockwordReservedShift + kProfileTimestampShift;
  const int64_t step = (1 << kProfileTimestampShift);
  const int64_t kMaxCycles = (int64_t{1} << kWrapShift) - step;

  // These bits should be zero after encoding.
  const uint32_t kLockwordReservedMask = (1 << kLockwordReservedShift) - 1;

  // These bits are dropped when timestamps are encoded.
  const int64_t kProfileTimestampMask = (1 << kProfileTimestampShift) - 1;

  // Test a bunch of random values
  std::default_random_engine generator;
  // Shift to avoid overflow below.
  std::uniform_int_distribution<int64_t> time_distribution(
      0, std::numeric_limits<int64_t>::max() >> 3);
  // Leave room for the rounding of start_time and end_time to differ.
  std::uniform_int_distribution<int64_t> cycle_distribution(0,
                                                            kMaxCycles - step);

  for (int i = 0; i < 100; i++) {
    int64_t start_time = time_distribution(generator);
    int64_t cycles = cycle_distribution(generator);
    int64_t end_time = start_time + cycles;
    uint32_t lock_value = SpinLockTest::EncodeWaitStart(start_time);
    EXPECT_EQ(0u, lock_value & kLockwordReservedMask);
    EXPECT_NE(0u, lock_value);
    if (lock_value == (1u << kLockwordReservedShift)) {
      // A timestamp whose truncation is zero encodes as one, costing one unit
      // of precision; skip the exact comparison below.
      continue;
    }
    int64_t decoded = SpinLockTest::DecodeWaitCycles(lock_value, end_time);
    EXPECT_EQ(0, decoded & kProfileTimestampMask);
    EXPECT_EQ((end_time >> kProfileTimestampShift) -
                  (start_time >> kProfileTimestampShift),
              decoded >> kProfileTimestampShift);
    EXPECT_LT(decoded, cycles + step);
    EXPECT_GT(decoded, cycles - step);
  }

  // Test corner cases
  int64_t start_time = time_distribution(generator);
  start_time &= ~kProfileTimestampMask;
  uint32_t lock_value = SpinLockTest::EncodeWaitStart(start_time);
  EXPECT_EQ(0, SpinLockTest::DecodeWaitCycles(lock_value, start_time));
  EXPECT_EQ(step,
            SpinLockTest::DecodeWaitCycles(lock_value, start_time + step));
  EXPECT_EQ(kMaxCycles, SpinLockTest::DecodeWaitCycles(
                            lock_value, start_time + kMaxCycles));
  // Once the clock wraps all the way around, the elapsed time wraps to zero.
  EXPECT_EQ(0, SpinLockTest::DecodeWaitCycles(lock_value,
                                              start_time + kMaxCycles + step));
  EXPECT_EQ(step, SpinLockTest::DecodeWaitCycles(
                      lock_value, start_time + kMaxCycles + 2 * step));

  // A timestamp whose truncation is zero must not encode as "no waiter", and
  // decoding in the same tick must return zero rather than underflowing.
  EXPECT_NE(0u, SpinLockTest::EncodeWaitStart(0));
  EXPECT_EQ(0, SpinLockTest::DecodeWaitCycles(SpinLockTest::EncodeWaitStart(0),
                                              0));
  EXPECT_NE(0u, SpinLockTest::EncodeWaitStart(int64_t{1} << kWrapShift));
  EXPECT_EQ(0, SpinLockTest::DecodeWaitCycles(
                   SpinLockTest::EncodeWaitStart(int64_t{1} << kWrapShift),
                   int64_t{1} << kWrapShift));
}

TEST(SpinLockWithThreads, StackSpinLock) {
  SpinLock spinlock;
  ThreadedTest(&spinlock);
}

TEST(SpinLockWithThreads, StackCooperativeSpinLock) {
  SpinLock spinlock;
  ThreadedTest(&spinlock);
}

TEST(SpinLockWithThreads, StackNonCooperativeSpinLock) {
  SpinLock spinlock(base_internal::SCHEDULE_KERNEL_ONLY);
  ThreadedTest(&spinlock);
}

TEST(SpinLockWithThreads, StaticCooperativeSpinLock) {
  ThreadedTest(&static_cooperative_spinlock);
}

TEST(SpinLockWithThreads, StaticNonCooperativeSpinLock) {
  ThreadedTest(&static_noncooperative_spinlock);
}

TEST(SpinLockWithThreads, DoesNotDeadlock) {
  struct Helper {
    static void NotifyThenLock(Notification* locked, SpinLock* spinlock,
                               BlockingCounter* b) {
      locked->WaitForNotification();  // Wait for LockThenWait() to hold "s".
      b->DecrementCount();
      SpinLockHolder l(*spinlock);
    }

    static void LockThenWait(Notification* locked, SpinLock* spinlock,
                             BlockingCounter* b) {
      SpinLockHolder l(*spinlock);
      locked->Notify();
      b->Wait();
    }

    static void DeadlockTest(SpinLock* spinlock, int num_spinners) {
      Notification locked;
      BlockingCounter counter(num_spinners);
      std::vector<std::thread> threads;

      threads.push_back(
          std::thread(Helper::LockThenWait, &locked, spinlock, &counter));
      for (int i = 0; i < num_spinners; ++i) {
        threads.push_back(
            std::thread(Helper::NotifyThenLock, &locked, spinlock, &counter));
      }

      for (auto& thread : threads) {
        thread.join();
      }
    }
  };

  int num_threads = base_internal::NumCPUs() * 2;
  SpinLock stack_cooperative_spinlock;
  SpinLock stack_noncooperative_spinlock(base_internal::SCHEDULE_KERNEL_ONLY);
  Helper::DeadlockTest(&stack_cooperative_spinlock, num_threads);
  Helper::DeadlockTest(&stack_noncooperative_spinlock, num_threads);
  Helper::DeadlockTest(&static_cooperative_spinlock, num_threads);
  Helper::DeadlockTest(&static_noncooperative_spinlock, num_threads);
}

TEST(SpinLockTest, IsCooperative) {
  SpinLock default_constructor;
  EXPECT_TRUE(SpinLockTest::IsCooperative(default_constructor));

  SpinLock cooperative;
  EXPECT_TRUE(SpinLockTest::IsCooperative(cooperative));

  SpinLock kernel_only(base_internal::SCHEDULE_KERNEL_ONLY);
  EXPECT_FALSE(SpinLockTest::IsCooperative(kernel_only));
}

TEST(SpinLockTest, ScopedLock) {
  SpinLock s;
  std::scoped_lock l(s);
}

}  // namespace
}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
