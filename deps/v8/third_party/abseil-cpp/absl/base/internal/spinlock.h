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
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/const_init.h"
#include "absl/base/internal/low_level_scheduling.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/tsan_mutex_interface.h"
#include "absl/base/macros.h"
#include "absl/base/thread_annotations.h"

namespace tcmalloc {
namespace tcmalloc_internal {

class AllocationGuardSpinLockHolder;

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
      // The wait_cycles value indicates how long this thread spent waiting
      // for the lock.
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

  // Store number of cycles between wait_start_time and wait_end_time in a
  // lock value.
  static uint32_t EncodeWaitCycles(int64_t wait_start_time,
                                   int64_t wait_end_time);

  // Extract number of wait cycles in a lock value.
  static int64_t DecodeWaitCycles(uint32_t lock_value);

  // Provide access to protected method above.  Use for testing only.
  friend struct SpinLockTest;
  friend class tcmalloc::tcmalloc_internal::AllocationGuardSpinLockHolder;

 private:
  // lockword_ is used to store the following:
  //
  // bit[0] encodes whether a lock is being held.
  // bit[1] encodes whether a lock uses cooperative scheduling.
  // bit[2] encodes whether the current lock holder disabled scheduling when
  //        acquiring the lock. Only set when kSpinLockHeld is also set.
  // bit[3:31] encodes time a lock spent on waiting as a 29-bit unsigned int.
  //        This is set by the lock holder to indicate how long it waited on
  //        the lock before eventually acquiring it. The number of cycles is
  //        encoded as a 29-bit unsigned int, or in the case that the current
  //        holder did not wait but another waiter is queued, the LSB
  //        (kSpinLockSleeper) is set. The implementation does not explicitly
  //        track the number of queued waiters beyond this. It must always be
  //        assumed that waiters may exist if the current holder was required to
  //        queue.
  //
  // Invariant: if the lock is not held, the value is either 0 or
  // kSpinLockCooperative.
  static constexpr uint32_t kSpinLockHeld = 1;
  static constexpr uint32_t kSpinLockCooperative = 2;
  static constexpr uint32_t kSpinLockDisabledScheduling = 4;
  static constexpr uint32_t kSpinLockSleeper = 8;
  // Includes kSpinLockSleeper.
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

  uint32_t TryLockInternal(uint32_t lock_value, uint32_t wait_cycles);
  void SlowLock() ABSL_ATTRIBUTE_COLD;
  void SlowUnlock(uint32_t lock_value) ABSL_ATTRIBUTE_COLD;
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
class ABSL_SCOPED_LOCKABLE [[nodiscard]] SpinLockHolder {
 public:
  inline explicit SpinLockHolder(
      SpinLock& l ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_EXCLUSIVE_LOCK_FUNCTION(l)
      : lock_(l) {
    l.lock();
  }
  ABSL_DEPRECATE_AND_INLINE()
  inline explicit SpinLockHolder(SpinLock* l) ABSL_EXCLUSIVE_LOCK_FUNCTION(l)
      : SpinLockHolder(*l) {}

  inline ~SpinLockHolder() ABSL_UNLOCK_FUNCTION() { lock_.unlock(); }

  SpinLockHolder(const SpinLockHolder&) = delete;
  SpinLockHolder& operator=(const SpinLockHolder&) = delete;

 private:
  SpinLock& lock_;
};

// Register a hook for profiling support.
//
// The function pointer registered here will be called whenever a spinlock is
// contended.  The callback is given an opaque handle to the contended spinlock
// and the number of wait cycles.  This is thread-safe, but only a single
// profiler can be registered.  It is an error to call this function multiple
// times with different arguments.
void RegisterSpinLockProfiler(void (*fn)(const void* lock,
                                         int64_t wait_cycles));

//------------------------------------------------------------------------------
// Public interface ends here.
//------------------------------------------------------------------------------

// If (result & kSpinLockHeld) == 0, then *this was successfully locked.
// Otherwise, returns last observed value for lockword_.
inline uint32_t SpinLock::TryLockInternal(uint32_t lock_value,
                                          uint32_t wait_cycles) {
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
          kSpinLockHeld | lock_value | wait_cycles | sched_disabled_bit,
          std::memory_order_acquire, std::memory_order_relaxed)) {
    SchedulingGuard::EnableRescheduling(sched_disabled_bit != 0);
  }

  return lock_value;
}

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_INTERNAL_SPINLOCK_H_
