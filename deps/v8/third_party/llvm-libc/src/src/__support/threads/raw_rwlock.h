//===--- Implementation of the RawRwLock class -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_RAW_RWLOCK_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_RAW_RWLOCK_H

#include "hdr/errno_macros.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/threads/raw_mutex.h"
#include "src/__support/threads/sleep.h"

#ifndef LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT
#define LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT 100
#endif

#ifdef LIBC_COPT_TIMEOUT_ENSURE_MONOTONICITY
#include "src/__support/time/monotonicity.h"
#endif

namespace LIBC_NAMESPACE_DECL {
// A namespace to rwlock specific utilities.
namespace rwlock {
// The role of the thread in the RwLock.
enum class Role { Reader = 0, Writer = 1 };

enum class LockResult : int {
  Success = 0,
  TimedOut = ETIMEDOUT,
  Overflow = EAGAIN,
  Busy = EBUSY,
  Deadlock = EDEADLOCK,
  PermissionDenied = EPERM,
};

// A waiting queue to keep track of the pending readers and writers.
class WaitingQueue final : private RawMutex {
  /* FutexWordType raw_mutex;  (from base class) */

  // Pending reader count (protected by the mutex)
  FutexWordType pending_readers;
  // Pending writer count (protected by the mutex)
  FutexWordType pending_writers;
  // Reader serialization (increases on each reader-waking operation)
  Futex reader_serialization;
  // Writer serialization (increases on each writer-waking operation)
  Futex writer_serialization;

public:
  // RAII guard to lock and unlock the waiting queue.
  class Guard {
    WaitingQueue &queue;
    bool is_pshared;

    LIBC_INLINE Guard(WaitingQueue &queue, bool is_pshared)
        : queue(queue), is_pshared(is_pshared) {
      queue.lock(cpp::nullopt, is_pshared);
    }

  public:
    LIBC_INLINE ~Guard() { queue.unlock(is_pshared); }
    template <Role role> LIBC_INLINE FutexWordType &pending_count() {
      if constexpr (role == Role::Reader)
        return queue.pending_readers;
      else
        return queue.pending_writers;
    }
    template <Role role> LIBC_INLINE FutexWordType &serialization() {
      if constexpr (role == Role::Reader)
        return queue.reader_serialization.val;
      else
        return queue.writer_serialization.val;
    }
    friend WaitingQueue;
  };

public:
  LIBC_INLINE constexpr WaitingQueue()
      : RawMutex(), pending_readers(0), pending_writers(0),
        reader_serialization(0), writer_serialization(0) {}

  LIBC_INLINE Guard acquire(bool is_pshared) {
    return Guard(*this, is_pshared);
  }

  template <Role role>
  LIBC_INLINE ErrorOr<int> wait(FutexWordType expected,
                                cpp::optional<Futex::Timeout> timeout,
                                bool is_pshared) {
    if constexpr (role == Role::Reader)
      return reader_serialization.wait(expected, timeout, is_pshared);
    else
      return writer_serialization.wait(expected, timeout, is_pshared);
  }

  template <Role role> LIBC_INLINE ErrorOr<int> notify(bool is_pshared) {
    if constexpr (role == Role::Reader)
      return reader_serialization.notify_all(is_pshared);
    else
      return writer_serialization.notify_one(is_pshared);
  }
};

// The RwState of the RwLock is stored in an integer word, consisting of the
// following components:
// -----------------------------------------------
// | Range    |           Description            |
// ===============================================
// | 0        | Pending Reader Bit               |
// -----------------------------------------------
// | 1        | Pending Writer Bit               |
// -----------------------------------------------
// | [2, MSB) | Active Reader Count              |
// -----------------------------------------------
// | MSB      | Active Writer Bit                |
// -----------------------------------------------
class RwState {
  // Shift amounts to access the components of the state.
  LIBC_INLINE_VAR static constexpr int PENDING_READER_SHIFT = 0;
  LIBC_INLINE_VAR static constexpr int PENDING_WRITER_SHIFT = 1;
  LIBC_INLINE_VAR static constexpr int ACTIVE_READER_SHIFT = 2;
  LIBC_INLINE_VAR static constexpr int ACTIVE_WRITER_SHIFT =
      cpp::numeric_limits<int>::digits;

  // Bitmasks to access the components of the state.
  LIBC_INLINE_VAR static constexpr int PENDING_READER_BIT =
      1 << PENDING_READER_SHIFT;
  LIBC_INLINE_VAR static constexpr int PENDING_WRITER_BIT =
      1 << PENDING_WRITER_SHIFT;
  LIBC_INLINE_VAR static constexpr int ACTIVE_READER_COUNT_UNIT =
      1 << ACTIVE_READER_SHIFT;
  LIBC_INLINE_VAR static constexpr int ACTIVE_WRITER_BIT =
      1 << ACTIVE_WRITER_SHIFT;
  LIBC_INLINE_VAR static constexpr int PENDING_MASK =
      PENDING_READER_BIT | PENDING_WRITER_BIT;

private:
  // We use the signed integer as the state type. It is easier
  // to reason about the state transitions using signness.
  int state;

public:
  // Construction and conversion functions.
  LIBC_INLINE constexpr RwState(int state = 0) : state(state) {}
  LIBC_INLINE constexpr operator int() const { return state; }

  // Utilities to check the state of the RwLock.
  LIBC_INLINE constexpr bool has_active_writer() const { return state < 0; }
  LIBC_INLINE constexpr bool has_active_reader() const {
    return state >= ACTIVE_READER_COUNT_UNIT;
  }
  LIBC_INLINE constexpr bool has_active_owner() const {
    return has_active_reader() || has_active_writer();
  }
  LIBC_INLINE constexpr bool has_last_reader() const {
    return (state >> ACTIVE_READER_SHIFT) == 1;
  }
  LIBC_INLINE constexpr bool has_pending_writer() const {
    return state & PENDING_WRITER_BIT;
  }
  LIBC_INLINE constexpr bool has_pending() const {
    return state & PENDING_MASK;
  }

  LIBC_INLINE constexpr RwState set_writer_bit() const {
    return RwState(state | ACTIVE_WRITER_BIT);
  }

  // The preference parameter changes the behavior of the lock acquisition
  // if there are both readers and writers waiting for the lock. If writers
  // are preferred, reader acquisition will be blocked until all pending
  // writers are served.
  template <Role role> LIBC_INLINE bool can_acquire(Role preference) const {
    if constexpr (role == Role::Reader) {
      switch (preference) {
      case Role::Reader:
        return !has_active_writer();
      case Role::Writer:
        return !has_active_writer() && !has_pending_writer();
      }
      __builtin_unreachable();
    } else
      return !has_active_owner();
  }

  // This function check if it is possible to grow the reader count without
  // overflowing the state.
  LIBC_INLINE cpp::optional<RwState> try_increase_reader_count() const {
    LIBC_ASSERT(!has_active_writer() &&
                "try_increase_reader_count shall only be called when there "
                "is no active writer.");
    RwState res;
    if (LIBC_UNLIKELY(__builtin_sadd_overflow(state, ACTIVE_READER_COUNT_UNIT,
                                              &res.state)))
      return cpp::nullopt;
    return res;
  }

  // Utilities to do atomic operations on the state.
  LIBC_INLINE static RwState fetch_sub_reader_count(cpp::Atomic<int> &target,
                                                    cpp::MemoryOrder order) {
    return RwState(target.fetch_sub(ACTIVE_READER_COUNT_UNIT, order));
  }

  LIBC_INLINE static RwState load(cpp::Atomic<int> &target,
                                  cpp::MemoryOrder order) {
    return RwState(target.load(order));
  }

  template <Role role>
  LIBC_INLINE static RwState fetch_set_pending_bit(cpp::Atomic<int> &target,
                                                   cpp::MemoryOrder order) {
    if constexpr (role == Role::Reader)
      return RwState(target.fetch_or(PENDING_READER_BIT, order));
    else
      return RwState(target.fetch_or(PENDING_WRITER_BIT, order));
  }
  template <Role role>
  LIBC_INLINE static RwState fetch_clear_pending_bit(cpp::Atomic<int> &target,
                                                     cpp::MemoryOrder order) {
    if constexpr (role == Role::Reader)
      return RwState(target.fetch_and(~PENDING_READER_BIT, order));
    else
      return RwState(target.fetch_and(~PENDING_WRITER_BIT, order));
  }

  LIBC_INLINE static RwState fetch_clear_active_writer(cpp::Atomic<int> &target,
                                                       cpp::MemoryOrder order) {
    return RwState(target.fetch_and(~ACTIVE_WRITER_BIT, order));
  }

  LIBC_INLINE bool compare_exchange_weak_with(cpp::Atomic<int> &target,
                                              RwState desired,
                                              cpp::MemoryOrder success_order,
                                              cpp::MemoryOrder failure_order) {
    return target.compare_exchange_weak(state, desired, success_order,
                                        failure_order);
  }

  // Utilities to spin and reload the state.
private:
  template <class F>
  LIBC_INLINE static RwState spin_reload_until(cpp::Atomic<int> &target,
                                               F &&func, unsigned spin_count) {
    for (;;) {
      auto state = RwState::load(target, cpp::MemoryOrder::RELAXED);
      if (func(state) || spin_count == 0)
        return state;
      sleep_briefly();
      spin_count--;
    }
  }

public:
  template <Role role>
  LIBC_INLINE static RwState spin_reload(cpp::Atomic<int> &target,
                                         Role preference, unsigned spin_count) {
    if constexpr (role == Role::Reader) {
      // Return the reader state if either the lock is available or there is
      // any ongoing contention.
      return spin_reload_until(
          target,
          [=](RwState state) {
            return state.can_acquire<Role::Reader>(preference) ||
                   state.has_pending();
          },
          spin_count);
    } else {
      // Return the writer state if either the lock is available or there is
      // any contention *between writers*. Since writers can be way less than
      // readers, we allow them to spin more to improve the fairness.
      return spin_reload_until(
          target,
          [=](RwState state) {
            return state.can_acquire<Role::Writer>(preference) ||
                   state.has_pending_writer();
          },
          spin_count);
    }
  }

  friend class RwLockTester;
};
} // namespace rwlock

class RawRwLock {
  using RwState = rwlock::RwState;
  using Role = rwlock::Role;
  using WaitingQueue = rwlock::WaitingQueue;

public:
  // Return types for the lock functions.
  // All the locking routines returning this type are marked as [[nodiscard]]
  // because it is a common error to assume the lock success without checking
  // the return value, which can lead to undefined behaviors or other subtle
  // bugs that are hard to reason about.
  using LockResult = rwlock::LockResult;

private:
  // Whether the RwLock is shared between processes.
  LIBC_PREFERED_TYPE(bool)
  unsigned is_pshared : 1;
  // Reader/Writer preference.
  LIBC_PREFERED_TYPE(Role)
  unsigned preference : 1;
  // RwState to keep track of the RwLock.
  cpp::Atomic<int> state;
  // Waiting queue to keep track of the  readers and writers.
  WaitingQueue queue;

private:
  // Load the bitfield preference.
  LIBC_INLINE Role get_preference() const {
    return static_cast<Role>(preference);
  }

  template <Role role> LIBC_INLINE LockResult try_lock(RwState &old) {
    if constexpr (role == Role::Reader) {
      while (LIBC_LIKELY(old.can_acquire<Role::Reader>(get_preference()))) {
        cpp::optional<RwState> next = old.try_increase_reader_count();
        if (!next)
          return LockResult::Overflow;
        if (LIBC_LIKELY(old.compare_exchange_weak_with(
                state, *next, cpp::MemoryOrder::ACQUIRE,
                cpp::MemoryOrder::RELAXED)))
          return LockResult::Success;
        // Notice that old is updated by the compare_exchange_weak_with
        // function.
      }
      return LockResult::Busy;
    } else {
      // This while loop should terminate quickly
      while (LIBC_LIKELY(old.can_acquire<Role::Writer>(get_preference()))) {
        if (LIBC_LIKELY(old.compare_exchange_weak_with(
                state, old.set_writer_bit(), cpp::MemoryOrder::ACQUIRE,
                cpp::MemoryOrder::RELAXED)))
          return LockResult::Success;
        // Notice that old is updated by the compare_exchange_weak_with
        // function.
      }
      return LockResult::Busy;
    }
  }

  template <Role role>
  LIBC_INLINE LockResult
  lock_slow(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
            unsigned spin_count = LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT) {
#ifdef LIBC_COPT_TIMEOUT_ENSURE_MONOTONICITY
    // Phase 2: convert the timeout if necessary.
    if (timeout)
      ensure_monotonicity(*timeout);
#endif

    // Phase 3: spin to get the initial state. We ignore the timing due to
    // spin since it should end quickly.
    RwState old =
        RwState::spin_reload<role>(state, get_preference(), spin_count);

    // Enter the main acquisition loop.
    for (;;) {
      // Phase 4: if the lock can be acquired, try to acquire it.
      LockResult result = try_lock<role>(old);
      if (result != LockResult::Busy)
        return result;

      // Phase 5: register ourselves as a  reader.
      int serial_number;
      {
        // The queue need to be protected by a mutex since the operations in
        // this block must be executed as a whole transaction. It is possible
        // that this lock will make the timeout imprecise, but this is the
        // best we can do. The transaction is small and everyone should make
        // progress rather quickly.
        WaitingQueue::Guard guard = queue.acquire(is_pshared);
        guard.template pending_count<role>()++;

        // Use atomic operation to guarantee the total order of the operations
        // on the state. The pending flag update should be visible to any
        // succeeding unlock events. Or, if a unlock does happen before we
        // sleep on the futex, we can avoid such waiting.
        old = RwState::fetch_set_pending_bit<role>(state,
                                                   cpp::MemoryOrder::RELAXED);
        // no need to use atomic since it is already protected by the mutex.
        serial_number = guard.serialization<role>();
      }

      // Phase 6: do futex wait until the lock is available or timeout is
      // reached.
      bool timeout_flag = false;
      if (!old.can_acquire<role>(get_preference())) {
        auto wait_result = queue.wait<role>(serial_number, timeout, is_pshared);
        timeout_flag = (!wait_result.has_value() && timeout.has_value());
      }

      // Phase 7: unregister ourselves as a pending reader/writer.
      {
        // Similarly, the unregister operation should also be an atomic
        // transaction.
        WaitingQueue::Guard guard = queue.acquire(is_pshared);
        guard.pending_count<role>()--;
        // Clear the flag if we are the last reader. The flag must be
        // cleared otherwise operations like trylock may fail even though
        // there is no competitors.
        if (guard.pending_count<role>() == 0)
          RwState::fetch_clear_pending_bit<role>(state,
                                                 cpp::MemoryOrder::RELAXED);
      }

      // Phase 8: exit the loop is timeout is reached.
      if (timeout_flag)
        return LockResult::TimedOut;

      // Phase 9: reload the state and retry the acquisition.
      old = RwState::spin_reload<role>(state, get_preference(), spin_count);
    }
  }

  // Compiler (clang 19.0) somehow decides that this function may be inlined,
  // which leads to a larger unlock function that is infeasible to be inlined.
  // Since notifcation routine is colder we mark it as noinline explicitly.
  [[gnu::noinline]]
  LIBC_INLINE void notify_pending_threads() {
    enum class WakeTarget { Readers, Writers, None };
    WakeTarget status;

    {
      WaitingQueue::Guard guard = queue.acquire(is_pshared);
      if (guard.pending_count<Role::Writer>() != 0) {
        guard.serialization<Role::Writer>()++;
        status = WakeTarget::Writers;
      } else if (guard.pending_count<Role::Reader>() != 0) {
        guard.serialization<Role::Reader>()++;
        status = WakeTarget::Readers;
      } else {
        status = WakeTarget::None;
      }
    }

    if (status == WakeTarget::Readers)
      queue.notify<Role::Reader>(is_pshared);
    else if (status == WakeTarget::Writers)
      queue.notify<Role::Writer>(is_pshared);
  }

public:
  LIBC_INLINE bool has_active_writer() {
    return RwState::load(state, cpp::MemoryOrder::RELAXED).has_active_writer();
  }

  LIBC_INLINE constexpr RawRwLock(Role preference = Role::Reader,
                                  bool is_pshared = false)
      : is_pshared(is_pshared),
        preference(static_cast<unsigned>(preference) & 1u), state(0), queue() {}

  [[nodiscard]]
  LIBC_INLINE LockResult try_read_lock() {
    RwState old = RwState::load(state, cpp::MemoryOrder::RELAXED);
    return try_lock<Role::Reader>(old);
  }

  [[nodiscard]]
  LIBC_INLINE LockResult try_write_lock() {
    RwState old = RwState::load(state, cpp::MemoryOrder::RELAXED);
    return try_lock<Role::Writer>(old);
  }

  [[nodiscard]]
  LIBC_INLINE LockResult
  read_lock(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
            unsigned spin_count = LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT) {
    LockResult result = try_read_lock();
    if (LIBC_LIKELY(result != LockResult::Busy))
      return result;
    return lock_slow<Role::Reader>(timeout, spin_count);
  }

  [[nodiscard]]
  LIBC_INLINE LockResult
  write_lock(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
             unsigned spin_count = LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT) {
    LockResult result = try_write_lock();
    if (LIBC_LIKELY(result != LockResult::Busy))
      return result;
    return lock_slow<Role::Writer>(timeout, spin_count);
  }

  [[nodiscard]]
  LIBC_INLINE LockResult unlock() {
    RwState old = RwState::load(state, cpp::MemoryOrder::RELAXED);
    if (old.has_active_writer()) {
      // The lock is held by a writer.
      // clear the writer bit.
      old =
          RwState::fetch_clear_active_writer(state, cpp::MemoryOrder::RELEASE);
      // If there is no pending readers or writers, we are done.
      if (!old.has_pending())
        return LockResult::Success;
    } else if (old.has_active_reader()) {
      // The lock is held by readers.
      // Decrease the reader count.
      old = RwState::fetch_sub_reader_count(state, cpp::MemoryOrder::RELEASE);
      // If there is no pending readers or writers, we are done.
      if (!old.has_last_reader() || !old.has_pending())
        return LockResult::Success;
    } else {
      return LockResult::PermissionDenied;
    }

    notify_pending_threads();
    return LockResult::Success;
  }

  [[nodiscard]]
  LIBC_INLINE LockResult check_for_destroy() {
    // We do not allocate any special resources for the RwLock, so this function
    // will only check if the lock is currently held by any thread.
    RwState old = RwState::load(state, cpp::MemoryOrder::RELAXED);
    if (old.has_active_owner())
      return LockResult::Busy;
    return LockResult::Success;
  }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_RAW_RWLOCK_H
