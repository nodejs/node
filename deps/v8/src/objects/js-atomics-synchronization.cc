// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-atomics-synchronization.h"

#include "src/base/macros.h"
#include "src/base/platform/yield-processor.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/waiter-queue-node.h"
#include "src/sandbox/external-pointer-inl.h"

namespace v8 {
namespace internal {

namespace detail {
class WaiterQueueNode;
}

namespace {
}  // namespace

namespace detail {

// The waiter queue lock guard provides a RAII-style mechanism for locking the
// waiter queue. It is a non copyable and non movable object and a new state
// must be set before destroying the guard.
class V8_NODISCARD WaiterQueueLockGuard final {
  using StateT = JSSynchronizationPrimitive::StateT;

 public:
  // Spinlock to acquire the IsWaiterQueueLockedField bit. current_state is
  // updated to the last value of the state before the waiter queue lock was
  // acquired.
  explicit WaiterQueueLockGuard(std::atomic<StateT>* state,
                                StateT& current_state)
      : state_(state), new_state_(kInvalidState) {
    while (!JSSynchronizationPrimitive::TryLockWaiterQueueExplicit(
        state, current_state)) {
      YIELD_PROCESSOR;
    }
  }

  // Constructor for creating a wrapper around a state whose waiter queue
  // is already locked and owned by this thread.
  explicit WaiterQueueLockGuard(std::atomic<StateT>* state, bool is_locked)
      : state_(state), new_state_(kInvalidState) {
    CHECK(is_locked);
    DCHECK(JSSynchronizationPrimitive::IsWaiterQueueLockedField::decode(
        state->load()));
  }

  WaiterQueueLockGuard(const WaiterQueueLockGuard&) = delete;
  WaiterQueueLockGuard() = delete;

  ~WaiterQueueLockGuard() {
    DCHECK_NOT_NULL(state_);
    DCHECK_NE(new_state_, kInvalidState);
    DCHECK(JSSynchronizationPrimitive::IsWaiterQueueLockedField::decode(
        state_->load()));
    new_state_ = JSSynchronizationPrimitive::IsWaiterQueueLockedField::update(
        new_state_, false);
    state_->store(new_state_, std::memory_order_release);
  }

  void set_new_state(StateT new_state) { new_state_ = new_state; }

  static std::optional<WaiterQueueLockGuard>
  NewAlreadyLockedWaiterQueueLockGuard(std::atomic<StateT>* state) {
    return std::optional<WaiterQueueLockGuard>(std::in_place, state, true);
  }

 private:
  static constexpr StateT kInvalidState =
      ~JSSynchronizationPrimitive::kEmptyState;
  std::atomic<StateT>* state_;
  StateT new_state_;
};

}  // namespace detail

// static
bool JSSynchronizationPrimitive::TryLockWaiterQueueExplicit(
    std::atomic<StateT>* state, StateT& expected) {
  // Try to acquire the queue lock.
  expected = IsWaiterQueueLockedField::update(expected, false);
  return state->compare_exchange_weak(
      expected, IsWaiterQueueLockedField::update(expected, true),
      std::memory_order_acquire, std::memory_order_relaxed);
}

// static
void JSSynchronizationPrimitive::SetWaiterQueueStateOnly(
    std::atomic<StateT>* state, StateT new_state) {
  // Set the new state changing only the waiter queue bits.
  DCHECK_EQ(new_state & ~kWaiterQueueMask, 0);
  StateT expected = state->load(std::memory_order_relaxed);
  StateT desired;
  do {
    desired = new_state | (expected & ~kWaiterQueueMask);
  } while (!state->compare_exchange_weak(
      expected, desired, std::memory_order_release, std::memory_order_relaxed));
}

Tagged<Object> JSSynchronizationPrimitive::NumWaitersForTesting(
    Isolate* requester) {
  DisallowGarbageCollection no_gc;
  std::atomic<StateT>* state = AtomicStatePtr();
  StateT current_state = state->load(std::memory_order_acquire);

  // There are no waiters.
  if (!HasWaitersField::decode(current_state)) return Smi::FromInt(0);

  int num_waiters;
  {
    // If this is counting the number of waiters on a mutex, the js mutex
    // can be taken by another thread without acquiring the queue lock. We
    // handle the state manually to release the queue lock without changing the
    // "is locked" bit.
    while (!TryLockWaiterQueueExplicit(state, current_state)) {
      YIELD_PROCESSOR;
    }

    if (!HasWaitersField::decode(current_state)) {
      // The queue was emptied while waiting for the queue lock.
      SetWaiterQueueStateOnly(state, kEmptyState);
      return Smi::FromInt(0);
    }

    // Get the waiter queue head.
    WaiterQueueNode* waiter_head = DestructivelyGetWaiterQueueHead(requester);
    DCHECK_NOT_NULL(waiter_head);
    num_waiters = WaiterQueueNode::LengthFromHead(waiter_head);

    // Release the queue lock and reinstall the same queue head by creating a
    // new state.
    DCHECK_EQ(state->load(),
              IsWaiterQueueLockedField::update(current_state, true));
    StateT new_state = SetWaiterQueueHead(requester, waiter_head, kEmptyState);
    new_state = IsWaiterQueueLockedField::update(new_state, false);
    SetWaiterQueueStateOnly(state, new_state);
  }

  return Smi::FromInt(num_waiters);
}

// TODO(https://crbug.com/42202493): Consider making and caching a canonical map
// for this result object, like we do for the iterator result object. static
DirectHandle<JSObject> JSAtomicsMutex::CreateResultObject(
    Isolate* isolate, DirectHandle<Object> value, bool success) {
  DirectHandle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->object_function());
  DirectHandle<Object> success_value = isolate->factory()->ToBoolean(success);
  JSObject::AddProperty(isolate, result, "value", value,
                        PropertyAttributes::NONE);
  JSObject::AddProperty(isolate, result, "success", success_value,
                        PropertyAttributes::NONE);
  return result;
}

// static
bool JSAtomicsMutex::TryLockExplicit(std::atomic<StateT>* state,
                                     StateT& expected) {
  // Try to lock a possibly contended mutex.
  expected = IsLockedField::update(expected, false);
  return state->compare_exchange_weak(
      expected, IsLockedField::update(expected, true),
      std::memory_order_acquire, std::memory_order_relaxed);
}

bool JSAtomicsMutex::BackoffTryLock(Isolate* requester,
                                    DirectHandle<JSAtomicsMutex> mutex,
                                    std::atomic<StateT>* state) {
  // The backoff algorithm is copied from PartitionAlloc's SpinningMutex.
  constexpr int kSpinCount = 64;
  constexpr int kMaxBackoff = 16;

  int tries = 0;
  int backoff = 1;
  StateT current_state = state->load(std::memory_order_relaxed);
  do {
    if (JSAtomicsMutex::TryLockExplicit(state, current_state)) return true;

    for (int yields = 0; yields < backoff; yields++) {
      YIELD_PROCESSOR;
      tries++;
    }

    backoff = std::min(kMaxBackoff, backoff << 1);
  } while (tries < kSpinCount);
  return false;
}

bool JSAtomicsMutex::MaybeEnqueueNode(Isolate* requester,
                                      DirectHandle<JSAtomicsMutex> mutex,
                                      std::atomic<StateT>* state,
                                      WaiterQueueNode* this_waiter) {
  DCHECK_NOT_NULL(this_waiter);
  // Try to acquire the queue lock, which is itself a spinlock.
  StateT current_state = state->load(std::memory_order_relaxed);
  std::optional<WaiterQueueLockGuard> waiter_queue_lock_guard =
      LockWaiterQueueOrJSMutex(state, current_state);
  if (!waiter_queue_lock_guard.has_value()) {
    // There is no waiter queue lock guard, so the lock was acquired.
    DCHECK(IsLockedField::decode(state->load()));
    return false;
  }

  // With the queue lock held, enqueue the requester onto the waiter queue.
  WaiterQueueNode* waiter_head =
      mutex->DestructivelyGetWaiterQueueHead(requester);
  WaiterQueueNode::Enqueue(&waiter_head, this_waiter);

  // Enqueue a new waiter queue head and release the queue lock.
  DCHECK_EQ(state->load(),
            IsWaiterQueueLockedField::update(current_state, true));
  StateT new_state =
      mutex->SetWaiterQueueHead(requester, waiter_head, current_state);
  // The lock is held, just not by us, so don't set the current thread id as
  // the owner.
  DCHECK(IsLockedField::decode(current_state));
  new_state = IsLockedField::update(new_state, true);
  waiter_queue_lock_guard->set_new_state(new_state);
  return true;
}

// static
std::optional<WaiterQueueLockGuard> JSAtomicsMutex::LockWaiterQueueOrJSMutex(
    std::atomic<StateT>* state, StateT& current_state) {
  for (;;) {
    if (IsLockedField::decode(current_state) &&
        TryLockWaiterQueueExplicit(state, current_state)) {
      return WaiterQueueLockGuard::NewAlreadyLockedWaiterQueueLockGuard(state);
    }
    // Also check for the lock having been released by another thread during
    // attempts to acquire the queue lock.
    if (TryLockExplicit(state, current_state)) return std::nullopt;
    YIELD_PROCESSOR;
  }
}

bool JSAtomicsMutex::LockJSMutexOrDequeueTimedOutWaiter(
    Isolate* requester, std::atomic<StateT>* state,
    WaiterQueueNode* timed_out_waiter) {
  // First acquire the queue lock, which is itself a spinlock.
  StateT current_state = state->load(std::memory_order_relaxed);
  // There are no waiters, but the js mutex lock may be held by another thread.
  if (!HasWaitersField::decode(current_state)) return false;

  // The details of updating the state in this function are too complicated
  // for the waiter queue lock guard to manage, so handle the state manually.
  while (!TryLockWaiterQueueExplicit(state, current_state)) {
    YIELD_PROCESSOR;
  }

  WaiterQueueNode* waiter_head = DestructivelyGetWaiterQueueHead(requester);

  if (waiter_head == nullptr) {
    // The queue is empty but the js mutex lock may be held by another thread,
    // release the waiter queue bit without changing the "is locked" bit.
    DCHECK(!HasWaitersField::decode(current_state));
    SetWaiterQueueStateOnly(state, kUnlockedUncontended);
    return false;
  }

  WaiterQueueNode* dequeued_node = WaiterQueueNode::DequeueMatching(
      &waiter_head,
      [&](WaiterQueueNode* node) { return node == timed_out_waiter; });

  // Release the queue lock and install the new waiter queue head.
  DCHECK_EQ(state->load(),
            IsWaiterQueueLockedField::update(current_state, true));
  StateT new_state = kUnlockedUncontended;
  new_state = SetWaiterQueueHead(requester, waiter_head, new_state);

  if (!dequeued_node) {
    // The timed out waiter was not in the queue, so it must have been dequeued
    // and notified between the time this thread woke up and the time it
    // acquired the queue lock, so there is a risk that the next queue head is
    // never notified. Try to take the js mutex lock here, if we succeed, the
    // next node will be notified by this thread, otherwise, it will be notified
    // by the thread holding the lock now.

    // Since we use strong CAS below, we know that the js mutex lock will be
    // held by either this thread or another thread that can't go through the
    // unlock fast path because this thread is holding the waiter queue lock.
    // Hence, it is safe to always set the "is locked" bit in new_state.
    new_state = IsLockedField::update(new_state, true);
    DCHECK(!IsWaiterQueueLockedField::decode(new_state));
    current_state = IsLockedField::update(current_state, false);
    if (state->compare_exchange_strong(current_state, new_state,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed)) {
      // The CAS atomically released the waiter queue lock and acquired the js
      // mutex lock.
      return true;
    }

    DCHECK(IsLockedField::decode(state->load()));
    state->store(new_state, std::memory_order_release);
    return false;
  }

  SetWaiterQueueStateOnly(state, new_state);
  return false;
}

// static
bool JSAtomicsMutex::LockSlowPath(Isolate* requester,
                                  DirectHandle<JSAtomicsMutex> mutex,
                                  std::atomic<StateT>* state,
                                  std::optional<base::TimeDelta> timeout) {
  for (;;) {
    // Spin for a little bit to try to acquire the lock, so as to be fast under
    // microcontention.
    if (BackoffTryLock(requester, mutex, state)) return true;

    // At this point the lock is considered contended, so try to go to sleep and
    // put the requester thread on the waiter queue.

    // Allocate a waiter queue node on-stack, since this thread is going to
    // sleep and will be blocked anyway.
    WaiterQueueNode this_waiter(requester);
    if (!MaybeEnqueueNode(requester, mutex, state, &this_waiter)) return true;

    bool rv;
    // Wait for another thread to release the lock and wake us up.
    if (timeout) {
      rv = this_waiter.WaitFor(*timeout);
      // Reload the state pointer after wake up in case of shared GC while
      // blocked.
      state = mutex->AtomicStatePtr();
      if (!rv) {
        // If timed out, remove ourself from the waiter list, which is usually
        // done by the thread performing the notifying.
        rv = mutex->LockJSMutexOrDequeueTimedOutWaiter(requester, state,
                                                       &this_waiter);
        return rv;
      }
    } else {
      this_waiter.Wait();
      // Reload the state pointer after wake up in case of shared GC while
      // blocked.
      state = mutex->AtomicStatePtr();
    }

    // After wake up we try to acquire the lock again by spinning, as the
    // contention at the point of going to sleep should not be correlated with
    // contention at the point of waking up.
  }
}

void JSAtomicsMutex::UnlockSlowPath(Isolate* requester,
                                    std::atomic<StateT>* state) {
  // The fast path unconditionally cleared the owner thread.
  DCHECK_EQ(ThreadId::Invalid().ToInteger(),
            AtomicOwnerThreadIdPtr()->load(std::memory_order_relaxed));

  // To wake a sleeping thread, first acquire the queue lock, which is itself
  // a spinlock.
  StateT current_state = state->load(std::memory_order_relaxed);
  WaiterQueueLockGuard waiter_queue_lock_guard(state, current_state);

  if (!HasWaitersField::decode(current_state)) {
    // All waiters were removed while waiting for the queue lock, possibly by
    // timing out. Release both the lock and the queue lock.
    StateT new_state = IsLockedField::update(current_state, false);
    waiter_queue_lock_guard.set_new_state(new_state);
    return;
  }

  WaiterQueueNode* waiter_head = DestructivelyGetWaiterQueueHead(requester);
  DCHECK_NOT_NULL(waiter_head);
  WaiterQueueNode* old_head = WaiterQueueNode::Dequeue(&waiter_head);

  // Release both the lock and the queue lock, and install the new waiter queue
  // head.
  StateT new_state = IsLockedField::update(current_state, false);
  new_state = SetWaiterQueueHead(requester, waiter_head, new_state);
  waiter_queue_lock_guard.set_new_state(new_state);

  old_head->Notify();
}

// static
void JSAtomicsCondition::QueueWaiter(Isolate* requester,
                                     DirectHandle<JSAtomicsCondition> cv,
                                     WaiterQueueNode* waiter) {
  // The state pointer should not be used outside of this block as a shared GC
  // may reallocate it after waiting.
  std::atomic<StateT>* state = cv->AtomicStatePtr();

  // Try to acquire the queue lock, which is itself a spinlock.
  StateT current_state = state->load(std::memory_order_relaxed);
  WaiterQueueLockGuard waiter_queue_lock_guard(state, current_state);

  // With the queue lock held, enqueue the requester onto the waiter queue.
  WaiterQueueNode* waiter_head = cv->DestructivelyGetWaiterQueueHead(requester);
  WaiterQueueNode::Enqueue(&waiter_head, waiter);

  // Release the queue lock and install the new waiter queue head.
  DCHECK_EQ(state->load(),
            IsWaiterQueueLockedField::update(current_state, true));
  StateT new_state =
      cv->SetWaiterQueueHead(requester, waiter_head, current_state);
  waiter_queue_lock_guard.set_new_state(new_state);
}

// static
bool JSAtomicsCondition::WaitFor(Isolate* requester,
                                 DirectHandle<JSAtomicsCondition> cv,
                                 DirectHandle<JSAtomicsMutex> mutex,
                                 std::optional<base::TimeDelta> timeout) {
  DisallowGarbageCollection no_gc;

  bool rv;
  {
    // Allocate a waiter queue node on-stack, since this thread is going to
    // sleep and will be blocked anyway.
    WaiterQueueNode this_waiter(requester);

    JSAtomicsCondition::QueueWaiter(requester, cv, &this_waiter);

    // Release the mutex and wait for another thread to wake us up, reacquiring
    // the mutex upon wakeup.
    mutex->Unlock(requester);
    if (timeout) {
      rv = this_waiter.WaitFor(*timeout);
      if (!rv) {
        // If timed out, remove ourself from the waiter list, which is usually
        // done by the thread performing the notifying.
        std::atomic<StateT>* state = cv->AtomicStatePtr();
        DequeueExplicit(
            requester, cv, state, [&](WaiterQueueNode** waiter_head) {
              WaiterQueueNode* dequeued = WaiterQueueNode::DequeueMatching(
                  waiter_head,
                  [&](WaiterQueueNode* node) { return node == &this_waiter; });
              return dequeued ? 1 : 0;
            });
      }
    } else {
      this_waiter.Wait();
      rv = true;
    }
  }
  JSAtomicsMutex::Lock(requester, mutex);
  return rv;
}

// static
uint32_t JSAtomicsCondition::DequeueExplicit(
    Isolate* requester, DirectHandle<JSAtomicsCondition> cv,
    std::atomic<StateT>* state, const DequeueAction& action_under_lock) {
  // First acquire the queue lock, which is itself a spinlock.
  StateT current_state = state->load(std::memory_order_relaxed);

  if (!HasWaitersField::decode(current_state)) return 0;
  WaiterQueueLockGuard waiter_queue_lock_guard(state, current_state);

  // Get the waiter queue head.
  WaiterQueueNode* waiter_head = cv->DestructivelyGetWaiterQueueHead(requester);

  // There's no waiter to wake up, release the queue lock by setting it to the
  // empty state.
  if (waiter_head == nullptr) {
    StateT new_state = kEmptyState;
    waiter_queue_lock_guard.set_new_state(new_state);
    return 0;
  }

  uint32_t num_dequeued_waiters = action_under_lock(&waiter_head);

  // Release the queue lock and install the new waiter queue head.
  DCHECK_EQ(state->load(),
            IsWaiterQueueLockedField::update(current_state, true));
  StateT new_state =
      cv->SetWaiterQueueHead(requester, waiter_head, current_state);
  waiter_queue_lock_guard.set_new_state(new_state);

  return num_dequeued_waiters;
}

// static
uint32_t JSAtomicsCondition::Notify(Isolate* requester,
                                    DirectHandle<JSAtomicsCondition> cv,
                                    uint32_t count) {
  std::atomic<StateT>* state = cv->AtomicStatePtr();

  // Dequeue count waiters.
  return DequeueExplicit(
      requester, cv, state, [=](WaiterQueueNode** waiter_head) -> uint32_t {
        WaiterQueueNode* old_head;
        if (count == 1) {
          old_head = WaiterQueueNode::Dequeue(waiter_head);
          if (!old_head) return 0;
          old_head->Notify();
          return 1;
        }
        if (count == kAllWaiters) {
          old_head = *waiter_head;
          *waiter_head = nullptr;
        } else {
          old_head = WaiterQueueNode::Split(waiter_head, count);
        }
        if (!old_head) return 0;
        // Notify while holding the queue lock to avoid notifying
        // waiters that have been deleted in other threads.
        return old_head->NotifyAllInList();
      });
}

}  // namespace internal
}  // namespace v8
