// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/futex-emulation.h"

#include <limits>

#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/handles-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/bigint.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

using AtomicsWaitEvent = v8::Isolate::AtomicsWaitEvent;

base::LazyMutex FutexEmulation::mutex_ = LAZY_MUTEX_INITIALIZER;
base::LazyInstance<FutexWaitList>::type FutexEmulation::wait_list_ =
    LAZY_INSTANCE_INITIALIZER;

void FutexWaitListNode::NotifyWake() {
  // Lock the FutexEmulation mutex before notifying. We know that the mutex
  // will have been unlocked if we are currently waiting on the condition
  // variable. The mutex will not be locked if FutexEmulation::Wait hasn't
  // locked it yet. In that case, we set the interrupted_
  // flag to true, which will be tested after the mutex locked by a future wait.
  base::MutexGuard lock_guard(FutexEmulation::mutex_.Pointer());
  // if not waiting, this will not have any effect.
  cond_.NotifyOne();
  interrupted_ = true;
}

FutexWaitList::FutexWaitList() : head_(nullptr), tail_(nullptr) {}

void FutexWaitList::AddNode(FutexWaitListNode* node) {
  DCHECK(node->prev_ == nullptr && node->next_ == nullptr);
  if (tail_) {
    tail_->next_ = node;
  } else {
    head_ = node;
  }

  node->prev_ = tail_;
  node->next_ = nullptr;
  tail_ = node;
}

void FutexWaitList::RemoveNode(FutexWaitListNode* node) {
  if (node->prev_) {
    node->prev_->next_ = node->next_;
  } else {
    head_ = node->next_;
  }

  if (node->next_) {
    node->next_->prev_ = node->prev_;
  } else {
    tail_ = node->prev_;
  }

  node->prev_ = node->next_ = nullptr;
}

void AtomicsWaitWakeHandle::Wake() {
  // Adding a separate `NotifyWake()` variant that doesn't acquire the lock
  // itself would likely just add unnecessary complexity..
  // The split lock by itself isn’t an issue, as long as the caller properly
  // synchronizes this with the closing `AtomicsWaitCallback`.
  {
    base::MutexGuard lock_guard(FutexEmulation::mutex_.Pointer());
    stopped_ = true;
  }
  isolate_->futex_wait_list_node()->NotifyWake();
}

enum WaitReturnValue : int { kOk = 0, kNotEqual = 1, kTimedOut = 2 };

namespace {

Object WaitJsTranslateReturn(Isolate* isolate, Object res) {
  if (res.IsSmi()) {
    int val = Smi::ToInt(res);
    switch (val) {
      case WaitReturnValue::kOk:
        return ReadOnlyRoots(isolate).ok_string();
      case WaitReturnValue::kNotEqual:
        return ReadOnlyRoots(isolate).not_equal_string();
      case WaitReturnValue::kTimedOut:
        return ReadOnlyRoots(isolate).timed_out_string();
      default:
        UNREACHABLE();
    }
  }
  return res;
}

}  // namespace

Object FutexEmulation::WaitJs32(Isolate* isolate,
                                Handle<JSArrayBuffer> array_buffer, size_t addr,
                                int32_t value, double rel_timeout_ms) {
  Object res =
      Wait<int32_t>(isolate, array_buffer, addr, value, rel_timeout_ms);
  return WaitJsTranslateReturn(isolate, res);
}

Object FutexEmulation::WaitJs64(Isolate* isolate,
                                Handle<JSArrayBuffer> array_buffer, size_t addr,
                                int64_t value, double rel_timeout_ms) {
  Object res =
      Wait<int64_t>(isolate, array_buffer, addr, value, rel_timeout_ms);
  return WaitJsTranslateReturn(isolate, res);
}

Object FutexEmulation::WaitWasm32(Isolate* isolate,
                                  Handle<JSArrayBuffer> array_buffer,
                                  size_t addr, int32_t value,
                                  int64_t rel_timeout_ns) {
  return Wait<int32_t>(isolate, array_buffer, addr, value, rel_timeout_ns >= 0,
                       rel_timeout_ns);
}

Object FutexEmulation::WaitWasm64(Isolate* isolate,
                                  Handle<JSArrayBuffer> array_buffer,
                                  size_t addr, int64_t value,
                                  int64_t rel_timeout_ns) {
  return Wait<int64_t>(isolate, array_buffer, addr, value, rel_timeout_ns >= 0,
                       rel_timeout_ns);
}

template <typename T>
Object FutexEmulation::Wait(Isolate* isolate,
                            Handle<JSArrayBuffer> array_buffer, size_t addr,
                            T value, double rel_timeout_ms) {
  DCHECK_LT(addr, array_buffer->byte_length());

  bool use_timeout = rel_timeout_ms != V8_INFINITY;
  int64_t rel_timeout_ns = -1;

  if (use_timeout) {
    // Convert to nanoseconds.
    double timeout_ns = rel_timeout_ms *
                        base::Time::kNanosecondsPerMicrosecond *
                        base::Time::kMicrosecondsPerMillisecond;
    if (timeout_ns > static_cast<double>(std::numeric_limits<int64_t>::max())) {
      // 2**63 nanoseconds is 292 years. Let's just treat anything greater as
      // infinite.
      use_timeout = false;
    } else {
      rel_timeout_ns = static_cast<int64_t>(timeout_ns);
    }
  }
  return Wait(isolate, array_buffer, addr, value, use_timeout, rel_timeout_ns);
}

namespace {
double WaitTimeoutInMs(double timeout_ns) {
  return timeout_ns < 0
             ? V8_INFINITY
             : timeout_ns / (base::Time::kNanosecondsPerMicrosecond *
                             base::Time::kMicrosecondsPerMillisecond);
}
}  // namespace

template <typename T>
Object FutexEmulation::Wait(Isolate* isolate,
                            Handle<JSArrayBuffer> array_buffer, size_t addr,
                            T value, bool use_timeout, int64_t rel_timeout_ns) {
  VMState<ATOMICS_WAIT> state(isolate);
  base::TimeDelta rel_timeout =
      base::TimeDelta::FromNanoseconds(rel_timeout_ns);

  // We have to convert the timeout back to double for the AtomicsWaitCallback.
  double rel_timeout_ms = WaitTimeoutInMs(static_cast<double>(rel_timeout_ns));
  AtomicsWaitWakeHandle stop_handle(isolate);

  isolate->RunAtomicsWaitCallback(AtomicsWaitEvent::kStartWait, array_buffer,
                                  addr, value, rel_timeout_ms, &stop_handle);

  if (isolate->has_scheduled_exception()) {
    return isolate->PromoteScheduledException();
  }

  Handle<Object> result;
  AtomicsWaitEvent callback_result = AtomicsWaitEvent::kWokenUp;

  do {  // Not really a loop, just makes it easier to break out early.
    base::MutexGuard lock_guard(mutex_.Pointer());
    std::shared_ptr<BackingStore> backing_store =
        array_buffer->GetBackingStore();
    DCHECK(backing_store);
    FutexWaitListNode* node = isolate->futex_wait_list_node();
    node->backing_store_ = backing_store;
    node->wait_addr_ = addr;
    node->waiting_ = true;

    // Reset node->waiting_ = false when leaving this scope (but while
    // still holding the lock).
    ResetWaitingOnScopeExit reset_waiting(node);

    T* p = reinterpret_cast<T*>(
        static_cast<int8_t*>(backing_store->buffer_start()) + addr);
    if (*p != value) {
      result = handle(Smi::FromInt(WaitReturnValue::kNotEqual), isolate);
      callback_result = AtomicsWaitEvent::kNotEqual;
      break;
    }

    base::TimeTicks timeout_time;
    base::TimeTicks current_time;

    if (use_timeout) {
      current_time = base::TimeTicks::Now();
      timeout_time = current_time + rel_timeout;
    }

    wait_list_.Pointer()->AddNode(node);

    while (true) {
      bool interrupted = node->interrupted_;
      node->interrupted_ = false;

      // Unlock the mutex here to prevent deadlock from lock ordering between
      // mutex_ and mutexes locked by HandleInterrupts.
      mutex_.Pointer()->Unlock();

      // Because the mutex is unlocked, we have to be careful about not dropping
      // an interrupt. The notification can happen in three different places:
      // 1) Before Wait is called: the notification will be dropped, but
      //    interrupted_ will be set to 1. This will be checked below.
      // 2) After interrupted has been checked here, but before mutex_ is
      //    acquired: interrupted is checked again below, with mutex_ locked.
      //    Because the wakeup signal also acquires mutex_, we know it will not
      //    be able to notify until mutex_ is released below, when waiting on
      //    the condition variable.
      // 3) After the mutex is released in the call to WaitFor(): this
      // notification will wake up the condition variable. node->waiting() will
      // be false, so we'll loop and then check interrupts.
      if (interrupted) {
        Object interrupt_object = isolate->stack_guard()->HandleInterrupts();
        if (interrupt_object.IsException(isolate)) {
          result = handle(interrupt_object, isolate);
          callback_result = AtomicsWaitEvent::kTerminatedExecution;
          mutex_.Pointer()->Lock();
          break;
        }
      }

      mutex_.Pointer()->Lock();

      if (node->interrupted_) {
        // An interrupt occurred while the mutex_ was unlocked. Don't wait yet.
        continue;
      }

      if (stop_handle.has_stopped()) {
        node->waiting_ = false;
        callback_result = AtomicsWaitEvent::kAPIStopped;
      }

      if (!node->waiting_) {
        result = handle(Smi::FromInt(WaitReturnValue::kOk), isolate);
        break;
      }

      // No interrupts, now wait.
      if (use_timeout) {
        current_time = base::TimeTicks::Now();
        if (current_time >= timeout_time) {
          result = handle(Smi::FromInt(WaitReturnValue::kTimedOut), isolate);
          callback_result = AtomicsWaitEvent::kTimedOut;
          break;
        }

        base::TimeDelta time_until_timeout = timeout_time - current_time;
        DCHECK_GE(time_until_timeout.InMicroseconds(), 0);
        bool wait_for_result =
            node->cond_.WaitFor(mutex_.Pointer(), time_until_timeout);
        USE(wait_for_result);
      } else {
        node->cond_.Wait(mutex_.Pointer());
      }

      // Spurious wakeup, interrupt or timeout.
    }

    wait_list_.Pointer()->RemoveNode(node);
  } while (false);

  isolate->RunAtomicsWaitCallback(callback_result, array_buffer, addr, value,
                                  rel_timeout_ms, nullptr);

  if (isolate->has_scheduled_exception()) {
    CHECK_NE(callback_result, AtomicsWaitEvent::kTerminatedExecution);
    result = handle(isolate->PromoteScheduledException(), isolate);
  }

  return *result;
}

Object FutexEmulation::Wake(Handle<JSArrayBuffer> array_buffer, size_t addr,
                            uint32_t num_waiters_to_wake) {
  DCHECK_LT(addr, array_buffer->byte_length());

  int waiters_woken = 0;
  std::shared_ptr<BackingStore> backing_store = array_buffer->GetBackingStore();

  base::MutexGuard lock_guard(mutex_.Pointer());
  FutexWaitListNode* node = wait_list_.Pointer()->head_;
  while (node && num_waiters_to_wake > 0) {
    std::shared_ptr<BackingStore> node_backing_store =
        node->backing_store_.lock();
    DCHECK(node_backing_store);
    if (backing_store.get() == node_backing_store.get() &&
        addr == node->wait_addr_ && node->waiting_) {
      node->waiting_ = false;
      node->cond_.NotifyOne();
      if (num_waiters_to_wake != kWakeAll) {
        --num_waiters_to_wake;
      }
      waiters_woken++;
    }

    node = node->next_;
  }

  return Smi::FromInt(waiters_woken);
}

Object FutexEmulation::NumWaitersForTesting(Handle<JSArrayBuffer> array_buffer,
                                            size_t addr) {
  DCHECK_LT(addr, array_buffer->byte_length());
  std::shared_ptr<BackingStore> backing_store = array_buffer->GetBackingStore();

  base::MutexGuard lock_guard(mutex_.Pointer());

  int waiters = 0;
  FutexWaitListNode* node = wait_list_.Pointer()->head_;
  while (node) {
    std::shared_ptr<BackingStore> node_backing_store =
        node->backing_store_.lock();
    DCHECK(node_backing_store);
    if (backing_store.get() == node_backing_store.get() &&
        addr == node->wait_addr_ && node->waiting_) {
      waiters++;
    }

    node = node->next_;
  }

  return Smi::FromInt(waiters);
}

}  // namespace internal
}  // namespace v8
