//
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
//

//  Most users requiring mutual exclusion should use Mutex.
//  SpinLock is provided for use in two situations:
//   - for use by Abseil internal code that Mutex itself depends on
//   - for async signal safety (see below)

// SpinLock with a SchedulingMode::SCHEDULE_KERNEL_ONLY is async
// signal safe. If a spinlock is used within a signal handler, all code that
// acquires the lock must ensure that the signal cannot arrive while they are
// holding the lock. Typically, this is done by blocking the signal.
//
// Threads waiting on a SpinLock may be woken in an arbitrary order.

#ifndef ABSL_BASE_INTERNAL_SPINLOCK_H_
#define ABSL_BASE_INTERNAL_SPINLOCK_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/const_init.h"
#include "absl/base/internal/low_level_scheduling.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/tsan_mutex_interface.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"

namespace tcmalloc {
namespace tcmalloc_internal {

class AllocationGuardSpinLockHolder;
class Static;

}  // namespace tcmalloc_internal
}  // namespace tcmalloc

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

class ABSL_LOCKABLE ABSL_ATTRIBUTE_WARN_UNUSED SpinLock {
 public:
  constexpr SpinLock() : lockword_(kSpinLockCooperative) { RegisterWithTsan(); }

  // Constructors that allow non-cooperative spinlocks to be created for use
  // inside thread schedulers.  Normal clients should not use these.
  constexpr explicit SpinLock(SchedulingMode mode)
      : lockword_(IsCooperative(mode) ? kSpinLockCooperative : 0) {
    RegisterWithTsan();
  }

#if ABSL_HAVE_ATTRIBUTE(enable_if) && !defined(_WIN32)
  // Constructor to inline users of the default scheduling mode.
  //
  // This only needs to exists for inliner runs, but doesn't work correctly in
  // clang+windows builds, likely due to mangling differences.
  ABSL_DEPRECATE_AND_INLINE()
  constexpr explicit SpinLock(SchedulingMode mode)
      __attribute__((enable_if(mode == SCHEDULE_COOPERATIVE_AND_KERNEL,
                               "Cooperative use default constructor")))
      : SpinLock() {}
#endif

  // Constructor for global SpinLock instances.  See absl/base/const_init.h.
  ABSL_DEPRECATE_AND_INLINE()
  constexpr SpinLock(absl::ConstInitType, SchedulingMode mode)
      : SpinLock(mode) {}

  // For global SpinLock instances prefer trivial destructor when possible.
  // Default but non-trivial destructor in some build configurations causes an
  // extra static initializer.
#ifdef ABSL_INTERNAL_HAVE_TSAN_INTERFACE
  ~SpinLock() { ABSL_TSAN_MUTEX_DESTROY(this, __tsan_mutex_not_static); }
#else
  ~SpinLock() = default;
#endif

  // Acquire this SpinLock.
  inline void lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() {
    ABSL_TSAN_MUTEX_PRE_LOCK(this, 0);
    if (!TryLockImpl()) {
      SlowLock();
    }
    ABSL_TSAN_MUTEX_POST_LOCK(this, 0, 0);
  }

  ABSL_DEPRECATE_AND_INLINE()
  inline void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() { return lock(); }

  // Try to acquire this SpinLock without blocking and return true if the
  // acquisition was successful.  If the lock was not acquired, false is
  // returned.  If this SpinLock is free at the time of the call, try_lock will
  // return true with high probability.
  [[nodiscard]] inline bool try_lock() ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    ABSL_TSAN_MUTEX_PRE_LOCK(this, __tsan_mutex_try_lock);
    bool res = TryLockImpl();
    ABSL_TSAN_MUTEX_POST_LOCK(
        this, __tsan_mutex_try_lock | (res ? 0 : __tsan_mutex_try_lock_failed),
        0);
    return res;
  }

  ABSL_DEPRECATE_AND_INLINE()
  [[nodiscard]] inline bool TryLock() ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return try_lock();
  }

  // Release this SpinLock, which must be held by the calling thread.
  inline void unlock() ABSL_UNLOCK_FUNCTION() {
    ABSL_TSAN_MUTEX_PRE_UNLOCK(this, 0);
    uint32_t lock_value = lockword_.load(std::memory_order_relaxed);
    lock_value = lockword_.exchange(lock_value & kSpinLockCooperative,
                                    std::memory_order_release);

    if ((lock_value & kSpinLockDisabledScheduling) != 0) {
      SchedulingGuard::EnableRescheduling(true);
    }
    if ((lock_value & kWaitTimeMask) != 0) {
      // Collect contentionz profile info, and speed the wakeup of any waiter.
      // The upper bits record when this thread's hold first had a sleeping
      // waiter.
      SlowUnlock(lock_value);
    }
    ABSL_TSAN_MUTEX_POST_UNLOCK(this, 0);
  }

  ABSL_DEPRECATE_AND_INLINE()
  inline void Unlock() ABSL_UNLOCK_FUNCTION() { unlock(); }

  // Determine if the lock is held.  When the lock is held by the invoking
  // thread, true will always be returned. Intended to be used as
  // CHECK(lock.IsHeld()).
  [[nodiscard]] inline bool IsHeld() const {
    return (lockword_.load(std::memory_order_relaxed) & kSpinLockHeld) != 0;
  }

  // Return immediately if this thread holds the SpinLock exclusively.
  // Otherwise, report an error by crashing with a diagnostic.
  inline void AssertHeld() const ABSL_ASSERT_EXCLUSIVE_LOCK() {
    if (!IsHeld()) {
      ABSL_RAW_LOG(FATAL, "thread should hold the lock on SpinLock");
    }
  }

 protected:
  // These should not be exported except for testing.

  // Encode the CycleClock timestamp `now` into the upper bits of a lock value.
  // The result is never zero.
  static uint32_t EncodeWaitStart(int64_t now);

  // Return the number of cycles between the timestamp encoded in a lock value
  // and `now`.  Must only be called when the lock value has an encoded wait
  // start.
  static int64_t DecodeWaitCycles(uint32_t lock_value, int64_t now);

  // Provide access to protected method above.  Use for testing only.
  friend struct SpinLockTest;
  friend struct SpinLockProfilingTestPeer;
  friend class tcmalloc::tcmalloc_internal::AllocationGuardSpinLockHolder;
  friend class tcmalloc::tcmalloc_internal::Static;

  static int GetAdaptiveSpinCount() {
    return adaptive_spin_count_.load(std::memory_order_relaxed);
  }
  static void SetAdaptiveSpinCount(int count) {
    adaptive_spin_count_.store(count, std::memory_order_relaxed);
  }

  static std::atomic<int> adaptive_spin_count_;

 private:
  // lockword_ is used to store the following:
  //
  // bit[0] encodes whether a lock is being held.
  // bit[1] encodes whether a lock uses cooperative scheduling.
  // bit[2] encodes whether the current lock holder disabled scheduling when
  //        acquiring the lock. Only set when kSpinLockHeld is also set.
  // bit[3:31] is non-zero iff a waiter may be sleeping in the kernel, in which
  //        case the holder must wake a waiter when it releases the lock.  The
  //        value is a 29-bit truncated CycleClock timestamp of when the
  //        current hold first had a sleeping waiter: set by the first waiter
  //        to go to sleep during the hold, or at acquisition if the holder
  //        itself slept while acquiring the lock.  Because unlock() clears
  //        these bits and wakes only one waiter without a sleeper count, a
  //        woken sleeper must conservatively keep them non-zero on acquisition
  //        in case other sleepers remain (which cannot observe the new hold).
  //        The holder uses this at unlock to report for how long its critical
  //        section blocked other threads; if the woken thread was the last
  //        sleeper, it reports its own hold once before its unlock() clears the
  //        bits.
  //
  // Invariant: if the lock is not held, the value is either 0 or
  // kSpinLockCooperative.
  static constexpr uint32_t kSpinLockHeld = 1;
  static constexpr uint32_t kSpinLockCooperative = 2;
  static constexpr uint32_t kSpinLockDisabledScheduling = 4;
  static constexpr uint32_t kWaitTimeMask =
      ~(kSpinLockHeld | kSpinLockCooperative | kSpinLockDisabledScheduling);

  // Returns true if the provided scheduling mode is cooperative.
  static constexpr bool IsCooperative(SchedulingMode scheduling_mode) {
    return scheduling_mode == SCHEDULE_COOPERATIVE_AND_KERNEL;
  }

  constexpr void RegisterWithTsan() {
#if ABSL_HAVE_BUILTIN(__builtin_is_constant_evaluated)
    if (!__builtin_is_constant_evaluated()) {
      ABSL_TSAN_MUTEX_CREATE(this, __tsan_mutex_not_static);
    }
#endif
  }

  bool IsCooperative() const {
    return lockword_.load(std::memory_order_relaxed) & kSpinLockCooperative;
  }

  // `sleeper_bits` is 0 or the result of EncodeWaitStart() and is set in the
  // lock word on a successful acquisition.
  uint32_t TryLockInternal(uint32_t lock_value, uint32_t sleeper_bits);
  ABSL_ATTRIBUTE_COLD void SlowLock();
  ABSL_ATTRIBUTE_COLD void SlowUnlock(uint32_t lock_value);
  uint32_t SpinLoop();

  inline bool TryLockImpl() {
    uint32_t lock_value = lockword_.load(std::memory_order_relaxed);
    return (TryLockInternal(lock_value, 0) & kSpinLockHeld) == 0;
  }

  std::atomic<uint32_t> lockword_;

  SpinLock(const SpinLock&) = delete;
  SpinLock& operator=(const SpinLock&) = delete;
};

// Corresponding locker object that arranges to acquire a spinlock for
// the duration of a C++ scope.
class ABSL_SCOPED_LOCKABLE [[nodiscard]] SpinLockHolder
    : public std::lock_guard<SpinLock> {
 public:
  inline explicit SpinLockHolder(
      SpinLock& l ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY_THIS)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(l)
      : std::lock_guard<SpinLock>(l) {}
  ABSL_DEPRECATE_AND_INLINE()
  inline explicit SpinLockHolder(SpinLock* absl_nonnull l)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(l)
      : SpinLockHolder(*l) {}

  inline ~SpinLockHolder() ABSL_UNLOCK_FUNCTION() = default;
};

// Register hooks for profiling support.
//
// The callbacks are given an opaque handle to the spinlock and a number of
// cycles as measured by //absl/base/internal/cycleclock.h, which
// may not be real "cycle" counts.  Registration is thread-safe, but only a
// single profiler can be registered for each hook.  It is an error to call
// these functions multiple times with different arguments.
//
// The function registered with RegisterSpinLockProfiler() is called when a
// spinlock is released after a contended hold, i.e. when at least one other
// thread went to sleep waiting for the releasing thread to release the lock.
// `wait_cycles` is the time elapsed since the first such thread went to sleep:
// approximately the duration for which the releasing thread's critical section
// blocked other threads.  Unlike Mutex, the number of blocked threads is not
// known, so this is not the sum of their waits; that sum is reported by the
// lock profiler below.
void RegisterSpinLockProfiler(void (*absl_nonnull fn)(
    const void* absl_nonnull lock, int64_t wait_cycles));

// The function registered with RegisterSpinLockLockProfiler() is called when a
// thread acquires a spinlock that it could not acquire by spinning.
// `wait_cycles` is the time for which the acquiring thread was blocked,
// measured from the point at which it gave up spinning.  Acquisitions that
// spinning resolves are not reported, so this undercounts the time spent
// waiting for spinlocks by at most one spin per acquisition.
void RegisterSpinLockLockProfiler(void (*absl_nonnull fn)(
    const void* absl_nonnull lock, int64_t wait_cycles));

//------------------------------------------------------------------------------
// Public interface ends here.
//------------------------------------------------------------------------------

// If (result & kSpinLockHeld) == 0, then *this was successfully locked.
// Otherwise, returns last observed value for lockword_.
inline uint32_t SpinLock::TryLockInternal(uint32_t lock_value,
                                          uint32_t sleeper_bits) {
  if ((lock_value & kSpinLockHeld) != 0) {
    return lock_value;
  }

  uint32_t sched_disabled_bit = 0;
  if ((lock_value & kSpinLockCooperative) == 0) {
    // For non-cooperative locks we must make sure we mark ourselves as
    // non-reschedulable before we attempt to CompareAndSwap.
    if (SchedulingGuard::DisableRescheduling()) {
      sched_disabled_bit = kSpinLockDisabledScheduling;
    }
  }

  if (!lockword_.compare_exchange_strong(
          lock_value,
          kSpinLockHeld | lock_value | sleeper_bits | sched_disabled_bit,
          std::memory_order_acquire, std::memory_order_relaxed)) {
    SchedulingGuard::EnableRescheduling(sched_disabled_bit != 0);
  }

  return lock_value;
}

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_INTERNAL_SPINLOCK_H_
