//===-- A platform independent abstraction layer for cond vars --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_CNDVAR_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_CNDVAR_H

#include "hdr/stdint_proxy.h" // uint32_t
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/mutex.h"
#include "src/__support/CPP/new.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/futex_utils.h" // Futex
#include "src/__support/threads/mutex.h"       // Mutex
#include "src/__support/threads/raw_mutex.h"   // RawMutex
#include "src/__support/threads/sleep.h"
#include "src/__support/time/abs_timeout.h"

#ifdef LIBC_COPT_TIMEOUT_ENSURE_MONOTONICITY
#include "src/__support/time/monotonicity.h"
#endif

namespace LIBC_NAMESPACE_DECL {

enum class CndVarResult {
  Success,
  MutexError,
  Timeout,
};

class CndVar {
public:
  using Timeout = internal::AbsTimeout;

private:
  // A single-waiter multiple-notifier barrier used to keep
  // track of cancellation threads. We use this barrier to
  // ensure in-queue threads that have posted their cancellation
  // request have finished dequeue themselves.
  class CancellationBarrier {
    LIBC_INLINE_VAR static constexpr size_t CANCEL_STEP = 2;
    LIBC_INLINE_VAR static constexpr size_t SLEEPING_BIT = 1;

    // LSB indicates whether the waiter is in sleeping state.
    Futex futex;

  public:
    LIBC_INLINE CancellationBarrier() : futex(0) {}
    // Add one more notification request.
    LIBC_INLINE void add_one() {
      futex.fetch_add(CANCEL_STEP, cpp::MemoryOrder::RELAXED);
    }
    // Send notification to one waiter.
    LIBC_INLINE void notify() {
      FutexWordType res = futex.fetch_sub(CANCEL_STEP);
      // Only need to goto syscall if waiter is sleep and we are the last one
      if (res <= (CANCEL_STEP | SLEEPING_BIT) && (res & SLEEPING_BIT) != 0)
        futex.notify_one();
    }
    LIBC_INLINE void wait() {
      size_t spin = 0;
      while (auto remaining = futex.load(cpp::MemoryOrder::RELAXED)) {
        // Set LSB to 1 to indicate that the waiter is entering sleeping
        // state.
        FutexWordType new_val = remaining | SLEEPING_BIT;
        if (spin > LIBC_COPT_RAW_MUTEX_DEFAULT_SPIN_COUNT &&
            futex.compare_exchange_strong(remaining, new_val)) {
          futex.wait(new_val, /*timeout=*/cpp::nullopt, /*is_pshared=*/false);
          futex.fetch_sub(1);
          spin = 0;
        }
        sleep_briefly();
        spin++;
      }
    }
  };

  enum WaiterState : uint8_t {
    Waiting = 0,
    Signalled = 1,
    Cancelled = 2,
    Requeued = 3,
  };

  template <typename T> struct QueueNode {
    T *prev;
    T *next;

    LIBC_INLINE T *self() { return static_cast<T *>(this); }

    // We use cyclic dummy node to avoid handing corner cases.
    LIBC_INLINE void ensure_queue_initialization() {
      if (LIBC_UNLIKELY(prev == nullptr))
        prev = next = self();
    }

    // Assume `this` the dummy node of queue. Push back `waiter` to the queue.
    LIBC_INLINE void push_back(T *waiter) {
      ensure_queue_initialization();
      waiter->next = self();
      waiter->prev = prev;
      waiter->next->prev = waiter;
      waiter->prev->next = waiter;
    }

    // Remove `waiter` from the queue.
    LIBC_INLINE static void remove(T *waiter) {
      waiter->next->prev = waiter->prev;
      waiter->prev->next = waiter->next;
      waiter->prev = waiter->next = waiter;
    }

    LIBC_INLINE bool is_empty() {
      ensure_queue_initialization();
      return self() == next;
    }

    // Assume `this` is the dummy node of the queue. Separate nodes before
    // cursor into a separate queue.
    LIBC_INLINE void separate(T *cursor) {
      T *removed_head = this->next;
      T *removed_tail = cursor->prev;
      this->next = cursor;
      cursor->prev = self();
      removed_tail->next = removed_head;
      removed_head->prev = removed_tail;
    }
  };

  // This node will be on the per-thread stack.
  struct CndWaiter : QueueNode<CndWaiter> {
    cpp::Atomic<CancellationBarrier *> cancellation_barrier;
    RawMutex barrier;
    cpp::Atomic<uint8_t> state;

    LIBC_INLINE CndWaiter()
        : QueueNode{}, cancellation_barrier(nullptr), barrier{},
          state{Waiting} {
      // this lock should always success as no contention is possible
      [[maybe_unused]] bool locked = barrier.try_lock();
      LIBC_ASSERT(locked);
    }

    LIBC_INLINE void confirm_cancellation() {
      if (CancellationBarrier *sender = cancellation_barrier.load())
        sender->notify();
    }
  };

  // Group structures with similar alignment together to
  // save trailing padding bytes, such that is_shared
  // can be introduced without extra space.
  union {
    QueueNode<CndWaiter> waiter_queue;
    cpp::Atomic<size_t> shared_waiters;
  };

  union {
    RawMutex queue_lock;
    Futex shared_futex;
  };

  const bool is_shared;
  const bool is_realtime;

  LIBC_INLINE void notify(bool is_broadcast) {
    if (LIBC_UNLIKELY(is_shared)) {
      if (shared_waiters.load() == 0)
        return;
      // increase the sequence number
      shared_futex.fetch_add(1);
      if (is_broadcast)
        shared_futex.notify_all(/*is_shared=*/true);
      else
        shared_futex.notify_one(/*is_shared=*/true);
      return;
    }

    size_t limit =
        is_broadcast ? cpp::numeric_limits<size_t>::max() : size_t{1};
    CancellationBarrier cancellation_barrier{};
    CndWaiter *head = nullptr;
    CndWaiter *cursor = nullptr;
    // Go through the queue, try send signal to waiters.
    // 1. if signal is sent, we reduce the number of pending signals
    // 2. if waiter cancelled before signal is sent, we add it
    //    to cancellation barrier and continue
    // Notice that cancelled sender will not continue before
    // we release the queue lock, because they also need to
    // acquire the lock and dequeue themselves.
    {
      cpp::lock_guard lock(queue_lock);
      if (waiter_queue.is_empty())
        return;
      for (cursor = waiter_queue.next; cursor != waiter_queue.self();
           cursor = cursor->next) {
        if (limit == 0)
          break;
        uint8_t expected = Waiting;
        if (!cursor->state.compare_exchange_strong(expected, Signalled)) {
          cancellation_barrier.add_one();
          cursor->cancellation_barrier.store(&cancellation_barrier);
          continue;
        }
        if (!head)
          head = cursor;
        limit--;
      }
      // remove everything before cursor
      waiter_queue.separate(cursor);
    }
    // We want to make sure the propagation queue contain only threads
    // that have consumed the signal. So we wait until all cancelled
    // finishing their dequeue operation.
    cancellation_barrier.wait();
    // Start propagate notification to the first waiter in the queue.
    // Waiters in the queue will acquire the lock in strict FIFO order:
    // Only when the predecessor has acquired the lock can the successor
    // be waken up to compete for the mutex.
    if (head)
      head->barrier.unlock();
  }

public:
  LIBC_INLINE constexpr CndVar(bool is_shared, bool is_realtime = false)
      : waiter_queue{}, queue_lock{}, is_shared(is_shared),
        is_realtime(is_realtime) {
    if (is_shared) {
      new (&shared_waiters) cpp::Atomic<size_t>(0);
      new (&shared_futex) Futex(0);
    }
  }

  LIBC_INLINE void reset() {
    if (is_shared) {
      shared_waiters.store(0);
      shared_futex.store(0);
      return;
    }
    queue_lock.reset();
    waiter_queue.prev = nullptr;
    waiter_queue.next = nullptr;
  }

  // The is_realtime field is just a field we spared for pthread_cond_t
  // It is not used in wait directly.
  LIBC_INLINE bool default_clock_is_realtime() const { return is_realtime; }

  // TODO: register callback for pthread cancellation
  LIBC_INLINE CndVarResult wait(Mutex *mutex,
                                cpp::optional<Timeout> timeout = cpp::nullopt) {
#ifdef LIBC_COPT_TIMEOUT_ENSURE_MONOTONICITY
    if (timeout)
      ensure_monotonicity(*timeout);
#endif

    if (LIBC_UNLIKELY(is_shared)) {
      shared_waiters.fetch_add(1);
      FutexWordType old_val = shared_futex.load();
      mutex->unlock();
      ErrorOr<int> result =
          shared_futex.wait(old_val, timeout, /*is_pshared=*/true);
      shared_waiters.fetch_sub(1);
      MutexError mutex_result = mutex->lock();
      if (!result.has_value() && result.error() == ETIMEDOUT)
        return CndVarResult::Timeout;
      return mutex_result == MutexError::NONE ? CndVarResult::Success
                                              : CndVarResult::MutexError;
    }

    CndWaiter waiter{};
    // Register the waiter to the queue.
    {
      cpp::lock_guard lock(queue_lock);
      waiter_queue.push_back(&waiter);
    }

    // Unlock the mutex and wait for the signal.
    mutex->unlock();
    // Notice that lock is already initialized as LOCKED. We abuse the LOCKED
    // state to indicate that the waiter is pending.
    bool locked = waiter.barrier.lock(timeout, /*is_shared=*/false);

    // if we wake up and find that we are still waiting, this means
    // timeout has been reached.
    uint8_t old_state = Waiting;
    if (waiter.state.compare_exchange_strong(old_state, Cancelled,
                                             cpp::MemoryOrder::ACQ_REL)) {
      // we haven't consumed the signal before timeout reaches.
      {
        cpp::lock_guard lock(queue_lock);
        CndWaiter::remove(&waiter);
      }
      waiter.confirm_cancellation();
    } else if (!locked) {
      // Whenever a signal is already consumed, we compete for the mutex
      // in the FIFO order of the queue. We only relock if we previously
      // wake up due to timeout. Otherwise, it means that our turn has
      // come, so we don't need to relock.
      waiter.barrier.lock();
    }

    // Reacquire the mutex lock. If error ever happens, we still wake up
    // our successor so that remaining waiters can continue. However, we treat
    // outselves as not owning the mutex and we don't touch the contention
    // bit.
    MutexError mutex_result = mutex->lock();
    // If we are requeued, we need to establish contention after lock, otherwise
    // requeued thread may clear the contention bit even though
    // there are still waiters behind it.
    if (mutex_result == MutexError::NONE &&
        waiter.state.load(cpp::MemoryOrder::RELAXED) == Requeued)
      mutex->get_raw_futex().store(RawMutex::IN_CONTENTION);
    // If there is other in the queue after us, we need to wake the next waiter.
    // If we cancelled, we should naturally have waiter.next == &waiter
    if (waiter.next != &waiter) {
      auto *next_waiter = waiter.next;
      CndWaiter::remove(&waiter);
      auto &next_barrier_futex = next_waiter->barrier.get_raw_futex();
      auto &mutex_futex = mutex->get_raw_futex();
      // the following is basically an inlined version of mutex::unlock
      // but with requeue instead of wake if it is possible.
      FutexWordType prev = next_barrier_futex.exchange(
          RawMutex::UNLOCKED, cpp::MemoryOrder::RELEASE);
      // If next waiter in queue sleeps, it will establish contention its own
      // barrier
      if (prev == RawMutex::IN_CONTENTION) {
        if (mutex_result == MutexError::NONE && mutex->can_be_requeued()) {
          ErrorOr<int> res = next_barrier_futex.requeue_to(
              mutex_futex, cpp::nullopt, /*wake_limit=*/0,
              /*requeue_limit=*/1,
              /*is_shared=*/false);
          if (!res.has_value()) // cannot requeue on this system
            next_waiter->barrier.wake(/*is_shared=*/false);
          else if (res.value() > 0) {
            next_waiter->state.store(Requeued, cpp::MemoryOrder::RELAXED);
            mutex->get_raw_futex().store(RawMutex::IN_CONTENTION);
          }
        } else { // cannot requeue under special lock mode
          next_waiter->barrier.wake(/*is_shared=*/false);
        }
      }
    }
    if (mutex_result != MutexError::NONE)
      return CndVarResult::MutexError;
    return old_state == Waiting ? CndVarResult::Timeout : CndVarResult::Success;
  }

  LIBC_INLINE void notify_one() { notify(/*is_broadcast=*/false); }
  LIBC_INLINE void broadcast() { notify(/*is_broadcast=*/true); }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_CNDVAR_H
