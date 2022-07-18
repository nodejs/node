// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-atomics-synchronization.h"

#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/yield-processor.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/parked-scope.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "src/sandbox/external-pointer-inl.h"

namespace v8 {
namespace internal {

namespace detail {

// To manage waiting threads, there is a process-wide doubly-linked intrusive
// list per waiter (i.e. mutex or condition variable). There is a per-thread
// node allocated on the stack when the thread goes to sleep during waiting. In
// the case of sandboxed pointers, the access to the on-stack node is indirected
// through the shared external pointer table.
class V8_NODISCARD WaiterQueueNode final {
 public:
  template <typename T>
  static typename T::StateT EncodeHead(Isolate* requester,
                                       WaiterQueueNode* head) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
    if (head == nullptr) return 0;
    auto state = static_cast<typename T::StateT>(
        requester->InsertWaiterQueueNodeIntoSharedExternalPointerTable(
            reinterpret_cast<Address>(head)));
#else
    auto state = base::bit_cast<typename T::StateT>(head);
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS

    DCHECK_EQ(0, state & T::kLockBitsMask);
    return state;
  }

  // Decode a WaiterQueueNode from the state. This is a destructive operation
  // when sandboxing external pointers to prevent reuse.
  template <typename T>
  static WaiterQueueNode* DestructivelyDecodeHead(Isolate* requester,
                                                  typename T::StateT state) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
    ExternalPointer_t ptr =
        static_cast<ExternalPointer_t>(state & T::kWaiterQueueHeadMask);
    if (ptr == 0) return nullptr;
    ExternalPointerHandle handle =
        static_cast<ExternalPointerHandle>(state & T::kWaiterQueueHeadMask);
    if (handle == 0) return nullptr;
    // The external pointer is cleared after decoding to prevent reuse by
    // multiple mutexes in case of heap corruption.
    return reinterpret_cast<WaiterQueueNode*>(
        requester->shared_external_pointer_table().Exchange(
            handle, kNullAddress, kWaiterQueueNodeTag));
#else
    return base::bit_cast<WaiterQueueNode*>(state & T::kWaiterQueueHeadMask);
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
  }

  // Enqueues {new_tail}, mutating {head} to be the new head.
  static void Enqueue(WaiterQueueNode** head, WaiterQueueNode* new_tail) {
    DCHECK_NOT_NULL(head);
    WaiterQueueNode* current_head = *head;
    if (current_head == nullptr) {
      new_tail->next_ = new_tail;
      new_tail->prev_ = new_tail;
      *head = new_tail;
    } else {
      WaiterQueueNode* current_tail = current_head->prev_;
      current_tail->next_ = new_tail;
      current_head->prev_ = new_tail;
      new_tail->next_ = current_head;
      new_tail->prev_ = current_tail;
    }
  }

  // Dequeues a waiter and returns it; {head} is mutated to be the new
  // head.
  static WaiterQueueNode* Dequeue(WaiterQueueNode** head) {
    DCHECK_NOT_NULL(head);
    DCHECK_NOT_NULL(*head);
    WaiterQueueNode* current_head = *head;
    WaiterQueueNode* new_head = current_head->next_;
    if (new_head == current_head) {
      *head = nullptr;
    } else {
      WaiterQueueNode* tail = current_head->prev_;
      new_head->prev_ = tail;
      tail->next_ = new_head;
      *head = new_head;
    }
    return current_head;
  }

  void Wait(Isolate* requester) {
    AllowGarbageCollection allow_before_parking;
    ParkedScope parked_scope(requester->main_thread_local_heap());
    base::MutexGuard guard(&wait_lock_);
    while (should_wait) {
      wait_cond_var_.Wait(&wait_lock_);
    }
  }

  void Notify() {
    base::MutexGuard guard(&wait_lock_);
    should_wait = false;
    wait_cond_var_.NotifyOne();
  }

  bool should_wait = false;

 private:
  // The queue wraps around, e.g. the head's prev is the tail, and the tail's
  // next is the head.
  WaiterQueueNode* next_ = nullptr;
  WaiterQueueNode* prev_ = nullptr;

  base::Mutex wait_lock_;
  base::ConditionVariable wait_cond_var_;
};

}  // namespace detail

using detail::WaiterQueueNode;

// static
Handle<JSAtomicsMutex> JSAtomicsMutex::Create(Isolate* isolate) {
  auto* factory = isolate->factory();
  Handle<Map> map = isolate->js_atomics_mutex_map();
  Handle<JSAtomicsMutex> mutex = Handle<JSAtomicsMutex>::cast(
      factory->NewSystemPointerAlignedJSObjectFromMap(
          map, AllocationType::kSharedOld));
  mutex->set_state(kUnlocked);
  mutex->set_owner_thread_id(ThreadId::Invalid().ToInteger());
  return mutex;
}

bool JSAtomicsMutex::TryLockExplicit(std::atomic<StateT>* state,
                                     StateT& expected) {
  // Try to lock a possibly contended mutex.
  expected &= ~kIsLockedBit;
  return state->compare_exchange_weak(expected, expected | kIsLockedBit,
                                      std::memory_order_acquire,
                                      std::memory_order_relaxed);
}

bool JSAtomicsMutex::TryLockWaiterQueueExplicit(std::atomic<StateT>* state,
                                                StateT& expected) {
  // The queue lock can only be acquired on a locked mutex.
  DCHECK(expected & kIsLockedBit);
  // Try to acquire the queue lock.
  expected &= ~kIsWaiterQueueLockedBit;
  return state->compare_exchange_weak(
      expected, expected | kIsWaiterQueueLockedBit, std::memory_order_acquire,
      std::memory_order_relaxed);
}

// static
void JSAtomicsMutex::LockSlowPath(Isolate* requester,
                                  Handle<JSAtomicsMutex> mutex,
                                  std::atomic<StateT>* state) {
  for (;;) {
    // Spin for a little bit to try to acquire the lock, so as to be fast under
    // microcontention.
    //
    // The backoff algorithm is copied from PartitionAlloc's SpinningMutex.
    constexpr int kSpinCount = 64;
    constexpr int kMaxBackoff = 16;

    int tries = 0;
    int backoff = 1;
    StateT current_state = state->load(std::memory_order_relaxed);
    do {
      if (mutex->TryLockExplicit(state, current_state)) return;

      for (int yields = 0; yields < backoff; yields++) {
        YIELD_PROCESSOR;
        tries++;
      }

      backoff = std::min(kMaxBackoff, backoff << 1);
    } while (tries < kSpinCount);

    // At this point the lock is considered contended, so try to go to sleep and
    // put the requester thread on the waiter queue.

    // Allocate a waiter queue node on-stack, since this thread is going to
    // sleep and will be blocked anyaway.
    WaiterQueueNode this_waiter;

    {
      // Try to acquire the queue lock, which is itself a spinlock.
      current_state = state->load(std::memory_order_relaxed);
      for (;;) {
        if ((current_state & kIsLockedBit) &&
            mutex->TryLockWaiterQueueExplicit(state, current_state)) {
          break;
        }
        // Also check for the lock having been released by another thread during
        // attempts to acquire the queue lock.
        if (mutex->TryLockExplicit(state, current_state)) return;
        YIELD_PROCESSOR;
      }

      // With the queue lock held, enqueue the requester onto the waiter queue.
      this_waiter.should_wait = true;
      WaiterQueueNode* waiter_head =
          WaiterQueueNode::DestructivelyDecodeHead<JSAtomicsMutex>(
              requester, current_state);
      WaiterQueueNode::Enqueue(&waiter_head, &this_waiter);

      // Release the queue lock and install the new waiter queue head by
      // creating a new state.
      DCHECK_EQ(state->load(), current_state | kIsWaiterQueueLockedBit);
      StateT new_state =
          WaiterQueueNode::EncodeHead<JSAtomicsMutex>(requester, waiter_head);
      // The lock is held, just not by us, so don't set the current thread id as
      // the owner.
      DCHECK(current_state & kIsLockedBit);
      DCHECK(!mutex->IsCurrentThreadOwner());
      new_state |= kIsLockedBit;
      state->store(new_state, std::memory_order_release);
    }

    // Wait for another thread to release the lock and wake us up.
    this_waiter.Wait(requester);

    // Reload the state pointer after wake up in case of shared GC while
    // blocked.
    state = mutex->AtomicStatePtr();

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
  while (!TryLockWaiterQueueExplicit(state, current_state)) {
    YIELD_PROCESSOR;
  }

  // Get the waiter queue head, which is guaranteed to be non-null because the
  // unlock fast path uses a strong CAS which does not allow spurious
  // failure. This is unlike the lock fast path, which uses a weak CAS.
  WaiterQueueNode* waiter_head =
      WaiterQueueNode::DestructivelyDecodeHead<JSAtomicsMutex>(requester,
                                                               current_state);
  WaiterQueueNode* old_head = WaiterQueueNode::Dequeue(&waiter_head);

  // Release both the lock and the queue lock and also install the new waiter
  // queue head by creating a new state.
  StateT new_state =
      WaiterQueueNode::EncodeHead<JSAtomicsMutex>(requester, waiter_head);
  state->store(new_state, std::memory_order_release);

  old_head->Notify();
}

}  // namespace internal
}  // namespace v8
