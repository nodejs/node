// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/futex-emulation.h"

#include <limits>

#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/conversions.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/list-inl.h"

namespace v8 {
namespace internal {

base::LazyMutex FutexEmulation::mutex_ = LAZY_MUTEX_INITIALIZER;
base::LazyInstance<FutexWaitList>::type FutexEmulation::wait_list_ =
    LAZY_INSTANCE_INITIALIZER;


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


Object* FutexEmulation::Wait(Isolate* isolate,
                             Handle<JSArrayBuffer> array_buffer, size_t addr,
                             int32_t value, double rel_timeout_ms) {
  // We never want to wait longer than this amount of time; this way we can
  // interrupt this thread even if this is an "infinitely blocking" wait.
  // TODO(binji): come up with a better way of interrupting only when
  // necessary, rather than busy-waiting.
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromMilliseconds(50);

  DCHECK(addr < NumberToSize(isolate, array_buffer->byte_length()));

  void* backing_store = array_buffer->backing_store();
  int32_t* p =
      reinterpret_cast<int32_t*>(static_cast<int8_t*>(backing_store) + addr);

  base::LockGuard<base::Mutex> lock_guard(mutex_.Pointer());

  if (*p != value) {
    return Smi::FromInt(Result::kNotEqual);
  }

  FutexWaitListNode* node = isolate->futex_wait_list_node();

  node->backing_store_ = backing_store;
  node->wait_addr_ = addr;
  node->waiting_ = true;

  bool use_timeout = rel_timeout_ms != V8_INFINITY;

  base::TimeDelta rel_timeout;
  if (use_timeout) {
    // Convert to nanoseconds.
    double rel_timeout_ns = rel_timeout_ms *
                            base::Time::kNanosecondsPerMicrosecond *
                            base::Time::kMicrosecondsPerMillisecond;
    if (rel_timeout_ns >
        static_cast<double>(std::numeric_limits<int64_t>::max())) {
      // 2**63 nanoseconds is 292 years. Let's just treat anything greater as
      // infinite.
      use_timeout = false;
    } else {
      rel_timeout = base::TimeDelta::FromNanoseconds(
          static_cast<int64_t>(rel_timeout_ns));
    }
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  base::TimeTicks timeout_time = start_time + rel_timeout;

  wait_list_.Pointer()->AddNode(node);

  Object* result;

  while (true) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    if (use_timeout && current_time > timeout_time) {
      result = Smi::FromInt(Result::kTimedOut);
      break;
    }

    base::TimeDelta time_until_timeout = timeout_time - current_time;
    base::TimeDelta time_to_wait =
        (use_timeout && time_until_timeout < kMaxWaitTime) ? time_until_timeout
                                                           : kMaxWaitTime;

    bool wait_for_result = node->cond_.WaitFor(mutex_.Pointer(), time_to_wait);
    USE(wait_for_result);

    if (!node->waiting_) {
      result = Smi::FromInt(Result::kOk);
      break;
    }

    // Spurious wakeup or timeout. Potentially handle interrupts before
    // continuing to wait.
    Object* interrupt_object = isolate->stack_guard()->HandleInterrupts();
    if (interrupt_object->IsException()) {
      result = interrupt_object;
      break;
    }
  }

  wait_list_.Pointer()->RemoveNode(node);

  return result;
}


Object* FutexEmulation::Wake(Isolate* isolate,
                             Handle<JSArrayBuffer> array_buffer, size_t addr,
                             int num_waiters_to_wake) {
  DCHECK(addr < NumberToSize(isolate, array_buffer->byte_length()));

  int waiters_woken = 0;
  void* backing_store = array_buffer->backing_store();

  base::LockGuard<base::Mutex> lock_guard(mutex_.Pointer());
  FutexWaitListNode* node = wait_list_.Pointer()->head_;
  while (node && num_waiters_to_wake > 0) {
    if (backing_store == node->backing_store_ && addr == node->wait_addr_) {
      node->waiting_ = false;
      node->cond_.NotifyOne();
      --num_waiters_to_wake;
      waiters_woken++;
    }

    node = node->next_;
  }

  return Smi::FromInt(waiters_woken);
}


Object* FutexEmulation::WakeOrRequeue(Isolate* isolate,
                                      Handle<JSArrayBuffer> array_buffer,
                                      size_t addr, int num_waiters_to_wake,
                                      int32_t value, size_t addr2) {
  DCHECK(addr < NumberToSize(isolate, array_buffer->byte_length()));
  DCHECK(addr2 < NumberToSize(isolate, array_buffer->byte_length()));

  void* backing_store = array_buffer->backing_store();
  int32_t* p =
      reinterpret_cast<int32_t*>(static_cast<int8_t*>(backing_store) + addr);

  base::LockGuard<base::Mutex> lock_guard(mutex_.Pointer());
  if (*p != value) {
    return Smi::FromInt(Result::kNotEqual);
  }

  // Wake |num_waiters_to_wake|
  int waiters_woken = 0;
  FutexWaitListNode* node = wait_list_.Pointer()->head_;
  while (node) {
    if (backing_store == node->backing_store_ && addr == node->wait_addr_) {
      if (num_waiters_to_wake > 0) {
        node->waiting_ = false;
        node->cond_.NotifyOne();
        --num_waiters_to_wake;
        waiters_woken++;
      } else {
        node->wait_addr_ = addr2;
      }
    }

    node = node->next_;
  }

  return Smi::FromInt(waiters_woken);
}


Object* FutexEmulation::NumWaitersForTesting(Isolate* isolate,
                                             Handle<JSArrayBuffer> array_buffer,
                                             size_t addr) {
  DCHECK(addr < NumberToSize(isolate, array_buffer->byte_length()));
  void* backing_store = array_buffer->backing_store();

  base::LockGuard<base::Mutex> lock_guard(mutex_.Pointer());

  int waiters = 0;
  FutexWaitListNode* node = wait_list_.Pointer()->head_;
  while (node) {
    if (backing_store == node->backing_store_ && addr == node->wait_addr_) {
      waiters++;
    }

    node = node->next_;
  }

  return Smi::FromInt(waiters);
}

}  // namespace internal
}  // namespace v8
