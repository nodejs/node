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

#include "absl/base/internal/spinlock.h"

#include <atomic>
#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/config.h"
#include "absl/base/internal/atomic_hook.h"
#include "absl/base/internal/cycleclock.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/spinlock_wait.h"
#include "absl/base/internal/sysinfo.h" /* For NumCPUs() */
#include "absl/base/internal/tsan_mutex_interface.h"
#include "absl/base/nullability.h"

// Description of lock-word:
//  31..00: [............................3][2][1][0]
//
//     [0]: kSpinLockHeld
//     [1]: kSpinLockCooperative
//     [2]: kSpinLockDisabledScheduling
// [31..3]: 0 OR
//          CycleClock timestamp >> PROFILE_TIMESTAMP_SHIFT
//
// Detailed descriptions:
//
// Bit [0]: The lock is considered held iff kSpinLockHeld is set.
//
// Bit [1]: Eligible waiters (e.g. Fibers) may co-operatively reschedule when
//          contended iff kSpinLockCooperative is set.
//
// Bit [2]: This bit is exclusive from bit [1].  It is used only by a
//          non-cooperative lock.  When set, indicates that scheduling was
//          successfully disabled when the lock was acquired.  May be unset,
//          even if non-cooperative, if a ThreadIdentity did not yet exist at
//          time of acquisition.
//
// Bits [31..3]: If non-zero, at least one waiter may be sleeping in the kernel
//          and the holder must wake a waiter when it releases the lock.  The
//          value is the time at which the current hold first had a sleeping
//          waiter: set by the first waiter to go to sleep during the hold, or
//          at acquisition by a holder that itself slept, since other sleepers
//          may remain.  At unlock, the holder reports the time elapsed since
//          then as the contended portion of its critical section.

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

ABSL_INTERNAL_ATOMIC_HOOK_ATTRIBUTES static AtomicHook<void (*)(
    const void* lock, int64_t wait_cycles)>
    submit_unlock_profile_data;

ABSL_INTERNAL_ATOMIC_HOOK_ATTRIBUTES static AtomicHook<void (*)(
    const void* lock, int64_t wait_cycles)>
    submit_lock_profile_data;

void RegisterSpinLockProfiler(void (*absl_nonnull fn)(
    const void* absl_nonnull contendedlock, int64_t wait_cycles)) {
  submit_unlock_profile_data.Store(fn);
}

void RegisterSpinLockLockProfiler(void (*absl_nonnull fn)(
    const void* absl_nonnull contendedlock, int64_t wait_cycles)) {
  submit_lock_profile_data.Store(fn);
}

// We use the upper 29 bits of the lock word to store the time at which the
// current hold first had a sleeping waiter.  Since the lower bits of the cycle
// counter wrap very quickly on high-frequency processors we divide to reduce
// the granularity to 2^kProfileTimestampShift sized units.  Because only 29
// bits are stored in the 32-bit lock word (and uncontended lock() does not read
// CycleClock), SlowUnlock only knows the start time modulo 2^36 CycleClock
// ticks (=~ 34 seconds at a 2 GHz CycleClock) and cannot distinguish a wrapped
// hold to saturate.
static constexpr int kProfileTimestampShift = 7;

// We currently reserve the lower 3 bits.
static constexpr int kLockwordReservedShift = 3;

// Waits shorter than one timestamp unit are not reported.
static constexpr int64_t kMinProfiledWaitCycles = int64_t{1}
                                                  << kProfileTimestampShift;

// Report how long an acquiring thread was blocked, measured from the point
// at which it gave up spinning for the lock.
static void ReportLockWait(SpinLock& lock, int64_t wait_start_time,
                           int64_t wait_end_time) {
  const int64_t wait_cycles = wait_end_time - wait_start_time;
  if (wait_cycles < kMinProfiledWaitCycles) {
    return;
  }
  ABSL_TSAN_MUTEX_PRE_DIVERT(&lock, 0);
  submit_lock_profile_data(&lock, wait_cycles);
  ABSL_TSAN_MUTEX_POST_DIVERT(&lock, 0);
}

// Monitor the lock to see if its value changes within some time period
// (adaptive_spin_count_ loop iterations). The last value read from the lock
// is returned from the method.
ABSL_CONST_INIT std::atomic<int> SpinLock::adaptive_spin_count_{0};
uint32_t SpinLock::SpinLoop() {
  // We are already in the slow path of SpinLock, initialize the
  // adaptive_spin_count here.
  if (adaptive_spin_count_.load(std::memory_order_relaxed) == 0) {
    int current_spin_count = 0;
    int new_spin_count = NumCPUs() > 1 ? 1000 : 1;
    // If this fails, the value will remain unchanged. We may not spin for the
    // intended duration, but that is still safe. We will try again on the next
    // call to SpinLoop.
    adaptive_spin_count_.compare_exchange_weak(
        current_spin_count, new_spin_count, std::memory_order_relaxed,
        std::memory_order_relaxed);
  }
  int c = adaptive_spin_count_.load(std::memory_order_relaxed);
  uint32_t lock_value;
  do {
    lock_value = lockword_.load(std::memory_order_relaxed);
  } while ((lock_value & kSpinLockHeld) != 0 && --c > 0);
  return lock_value;
}

void SpinLock::SlowLock() {
  uint32_t lock_value = SpinLoop();
  lock_value = TryLockInternal(lock_value, 0);
  if ((lock_value & kSpinLockHeld) == 0) {
    return;
  }

  SchedulingMode scheduling_mode;
  if ((lock_value & kSpinLockCooperative) != 0) {
    scheduling_mode = SCHEDULE_COOPERATIVE_AND_KERNEL;
  } else {
    scheduling_mode = SCHEDULE_KERNEL_ONLY;
  }

  // The lock was not obtained initially, so this thread needs to wait for
  // it.  Record the current timestamp in the local variable wait_start_time
  // so the total wait time can be reported once this thread obtains the lock.
  //
  // Once this thread has slept, it must also store a wait start time in the
  // lock word when it eventually acquires the lock, since other sleepers may
  // remain that only the next unlock can wake.  Those sleepers cannot observe
  // the new hold, so the acquiring thread records its own acquisition as the
  // start of contention on behalf of them.
  const int64_t wait_start_time = CycleClock::Now();
  int64_t now = wait_start_time;
  uint32_t sleeper_bits = 0;
  int lock_wait_call_count = 0;
  while ((lock_value & kSpinLockHeld) != 0) {
    // If the lock is currently held, but not marked as having a sleeper, mark
    // it as having a sleeper.
    if ((lock_value & kWaitTimeMask) == 0) {
      // Here, "mark" that the thread is going to sleep by recording when the
      // current hold first had a sleeping waiter.
      const uint32_t new_value = lock_value | EncodeWaitStart(now);
      if (lockword_.compare_exchange_strong(lock_value, new_value,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
        // Successfully marked as having a sleeper.  Pass the new value to the
        // SpinLockWait routine to properly indicate the last lock_value
        // observed.
        lock_value = new_value;
      } else if ((lock_value & kSpinLockHeld) == 0) {
        // Lock is free again, so try and acquire it before sleeping.
        lock_value = TryLockInternal(lock_value, sleeper_bits);
        continue;  // Skip the delay at the end of the loop.
      } else if ((lock_value & kWaitTimeMask) == 0) {
        // The lock is still held, without a waiter being marked, but something
        // else about the lock word changed, causing our CAS to fail. For
        // example, a new lock holder may have acquired the lock with
        // kSpinLockDisabledScheduling set, whereas the previous holder had not
        // set that flag. In this case, attempt again to mark ourselves as a
        // waiter.
        continue;
      }
    }

    // SpinLockDelay() calls into fiber scheduler, we need to see
    // synchronization there to avoid false positives.
    ABSL_TSAN_MUTEX_PRE_DIVERT(this, 0);
    // Wait for an OS specific delay.
    SpinLockDelay(&lockword_, lock_value, ++lock_wait_call_count,
                  scheduling_mode);
    ABSL_TSAN_MUTEX_POST_DIVERT(this, 0);
    // Spin again after returning from the wait routine to give this thread
    // some chance of obtaining the lock.
    lock_value = SpinLoop();
    now = CycleClock::Now();
    sleeper_bits = EncodeWaitStart(now);
    lock_value = TryLockInternal(lock_value, sleeper_bits);
  }

  ReportLockWait(*this, wait_start_time, now);
}

void SpinLock::SlowUnlock(uint32_t lock_value) {
  // Read the clock before waking a waiter, which may preempt this thread.
  const int64_t now = CycleClock::Now();
  SpinLockWake(&lockword_,
               false);  // wake waiter if necessary

  // Collect contentionz profile info: for how long our critical section had a
  // sleeping waiter, attributed to our stack.
  const int64_t wait_cycles = DecodeWaitCycles(lock_value, now);
  if (wait_cycles < kMinProfiledWaitCycles) {
    return;
  }
  ABSL_TSAN_MUTEX_PRE_DIVERT(this, 0);
  submit_unlock_profile_data(this, wait_cycles);
  ABSL_TSAN_MUTEX_POST_DIVERT(this, 0);
}

uint32_t SpinLock::EncodeWaitStart(int64_t now) {
  uint32_t scaled_now = static_cast<uint32_t>(now >> kProfileTimestampShift) &
                        (kWaitTimeMask >> kLockwordReservedShift);
  // Zero in kWaitTimeMask means the current hold has no sleeping waiter.  If
  // `now` was truncated to zero, we round to the nearest positive value to
  // ensure that any sleeping waiters will be woken when the lock is released.
  if (scaled_now == 0) {
    scaled_now = 1;
  }
  return scaled_now << kLockwordReservedShift;
}

int64_t SpinLock::DecodeWaitCycles(uint32_t lock_value, int64_t now) {
  // Modular subtraction in the width of the stored timestamp.  Encode `now`
  // the same way as `lock_value` so a timestamp truncated to zero does not
  // underflow against EncodeWaitStart's rounding to one.
  const uint32_t scaled_wait_time =
      (EncodeWaitStart(now) - (lock_value & kWaitTimeMask)) >>
      kLockwordReservedShift;
  return static_cast<int64_t>(scaled_wait_time) << kProfileTimestampShift;
}

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
