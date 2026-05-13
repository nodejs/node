//===--- Implementation of the RawMutex class -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_RAW_MUTEX_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_RAW_MUTEX_H

#include "hdr/errno_macros.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/threads/futex_utils.h"
#include "src/__support/threads/sleep.h"
#include "src/__support/time/abs_timeout.h"

#include <stdio.h>

// TODO(bojle): check this for darwin impl
#ifdef LIBC_COPT_TIMEOUT_ENSURE_MONOTONICITY
#include "src/__support/time/monotonicity.h"
#endif

#ifndef LIBC_COPT_RAW_MUTEX_DEFAULT_SPIN_COUNT
#define LIBC_COPT_RAW_MUTEX_DEFAULT_SPIN_COUNT 100
#endif

namespace LIBC_NAMESPACE_DECL {
// Lock is a simple timable lock for internal usage.
// This is separated from Mutex because this one does not need to consider
// robustness and reentrancy. Also, this one has spin optimization for shorter
// critical sections.
class RawMutex {
protected:
  Futex futex;
  LIBC_INLINE_VAR static constexpr FutexWordType UNLOCKED = 0b00;
  LIBC_INLINE_VAR static constexpr FutexWordType LOCKED = 0b01;
  LIBC_INLINE_VAR static constexpr FutexWordType IN_CONTENTION = 0b10;
  friend class CndVar;

private:
  LIBC_INLINE FutexWordType spin(unsigned spin_count) {
    FutexWordType result;
    for (;;) {
      result = futex.load(cpp::MemoryOrder::RELAXED);
      // spin until one of the following conditions is met:
      // - the mutex is unlocked
      // - the mutex is in contention
      // - the spin count reaches 0
      if (result != LOCKED || spin_count == 0u)
        return result;
      // Pause the pipeline to avoid extraneous memory operations due to
      // speculation.
      sleep_briefly();
      spin_count--;
    };
  }

  // Return true if the lock is acquired. Return false if timeout happens before
  // the lock is acquired.
  LIBC_INLINE bool lock_slow(cpp::optional<Futex::Timeout> timeout,
                             bool is_pshared, unsigned spin_count) {
    FutexWordType state = spin(spin_count);
    // Before go into contention state, try to grab the lock.
    if (state == UNLOCKED &&
        futex.compare_exchange_strong(state, LOCKED, cpp::MemoryOrder::ACQUIRE,
                                      cpp::MemoryOrder::RELAXED))
      return true;
#ifdef LIBC_COPT_TIMEOUT_ENSURE_MONOTONICITY
    /* ADL should kick in */
    if (timeout)
      ensure_monotonicity(*timeout);
#endif
    for (;;) {
      // Try to grab the lock if it is unlocked. Mark the contention flag if it
      // is locked.
      if (state != IN_CONTENTION &&
          futex.exchange(IN_CONTENTION, cpp::MemoryOrder::ACQUIRE) == UNLOCKED)
        return true;
      // Contention persists. Park the thread and wait for further notification.
      if (!futex.wait(IN_CONTENTION, timeout, is_pshared).has_value() &&
          timeout.has_value())
        return false;

      // Continue to spin after waking up.
      state = spin(spin_count);
    }
  }

  LIBC_INLINE void wake(bool is_pshared) { futex.notify_one(is_pshared); }

public:
  LIBC_INLINE static void init(RawMutex *mutex) {
    mutex->futex.store(UNLOCKED);
  }
  LIBC_INLINE constexpr RawMutex() : futex(UNLOCKED) {}
  [[nodiscard]] LIBC_INLINE bool try_lock() {
    FutexWordType expected = UNLOCKED;
    // Use strong version since this is a one-time operation.
    return futex.compare_exchange_strong(
        expected, LOCKED, cpp::MemoryOrder::ACQUIRE, cpp::MemoryOrder::RELAXED);
  }
  LIBC_INLINE bool
  lock(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
       bool is_shared = false,
       unsigned spin_count = LIBC_COPT_RAW_MUTEX_DEFAULT_SPIN_COUNT) {
    // Timeout will not be checked if immediate lock is possible.
    if (LIBC_LIKELY(try_lock()))
      return true;
    return lock_slow(timeout, is_shared, spin_count);
  }
  LIBC_INLINE bool unlock(bool is_pshared = false) {
    FutexWordType prev = futex.exchange(UNLOCKED, cpp::MemoryOrder::RELEASE);
    // if there is someone waiting, wake them up
    if (LIBC_UNLIKELY(prev == IN_CONTENTION))
      wake(is_pshared);
    // Detect invalid unlock operation.
    return prev != UNLOCKED;
  }
  LIBC_INLINE void static destroy([[maybe_unused]] RawMutex *lock) {
    LIBC_ASSERT(lock->futex == UNLOCKED && "Mutex destroyed while used.");
  }
  LIBC_INLINE Futex &get_raw_futex() { return futex; }
  LIBC_INLINE void reset() { futex.store(UNLOCKED); }
};
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_RAW_MUTEX_H
