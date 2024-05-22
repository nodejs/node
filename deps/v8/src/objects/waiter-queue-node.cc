// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/waiter-queue-node.h"

#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/heap/local-heap-inl.h"

namespace v8 {
namespace internal {
namespace detail {

WaiterQueueNode::WaiterQueueNode(Isolate* requester) : requester_(requester) {}

WaiterQueueNode::~WaiterQueueNode() {
  // Since waiter queue nodes are allocated on the stack, they must be removed
  // from the intrusive linked list once they go out of scope, otherwise there
  // will be dangling pointers.
  VerifyNotInList();
}

// static
void WaiterQueueNode::Enqueue(WaiterQueueNode** head,
                              WaiterQueueNode* new_tail) {
  DCHECK_NOT_NULL(head);
  new_tail->VerifyNotInList();
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

WaiterQueueNode* WaiterQueueNode::DequeueMatching(
    WaiterQueueNode** head, const DequeueMatcher& matcher) {
  DCHECK_NOT_NULL(head);
  DCHECK_NOT_NULL(*head);
  WaiterQueueNode* original_head = *head;
  WaiterQueueNode* cur = *head;
  do {
    if (matcher(cur)) {
      WaiterQueueNode* next = cur->next_;
      if (next == cur) {
        // The queue contains exactly 1 node.
        *head = nullptr;
      } else {
        // The queue contains >1 nodes.
        if (cur == original_head) {
          // The matched node is the original head, so next is the new head.
          WaiterQueueNode* tail = original_head->prev_;
          next->prev_ = tail;
          tail->next_ = next;
          *head = next;
        } else {
          // The matched node is in the middle of the queue, so the head does
          // not need to be updated.
          cur->prev_->next_ = next;
          next->prev_ = cur->prev_;
        }
      }
      cur->SetNotInListForVerification();
      return cur;
    }
    cur = cur->next_;
  } while (cur != original_head);
  return nullptr;
}

// static
WaiterQueueNode* WaiterQueueNode::Dequeue(WaiterQueueNode** head) {
  return DequeueMatching(head, [](WaiterQueueNode* node) { return true; });
}

// static
// Splits at most {count} nodes of the waiter list of into its own list and
// returns it, mutating {head} to be the head of the back list.
WaiterQueueNode* WaiterQueueNode::Split(WaiterQueueNode** head,
                                        uint32_t count) {
  DCHECK_GT(count, 0);
  DCHECK_NOT_NULL(head);
  DCHECK_NOT_NULL(*head);
  WaiterQueueNode* front_head = *head;
  WaiterQueueNode* back_head = front_head;
  uint32_t actual_count = 0;
  while (actual_count < count) {
    back_head = back_head->next_;
    // The queue is shorter than the requested count, return the whole queue.
    if (back_head == front_head) {
      *head = nullptr;
      return front_head;
    }
    actual_count++;
  }
  WaiterQueueNode* front_tail = back_head->prev_;
  WaiterQueueNode* back_tail = front_head->prev_;

  // Fix up the back list (i.e. remainder of the list).
  back_head->prev_ = back_tail;
  back_tail->next_ = back_head;
  *head = back_head;

  // Fix up and return the front list (i.e. the dequeued list).
  front_head->prev_ = front_tail;
  front_tail->next_ = front_head;
  return front_head;
}

// static
// This method must be called from a known waiter queue head. Incorrectly
// encoded lists can cause this method to infinitely loop.
int WaiterQueueNode::LengthFromHead(WaiterQueueNode* head) {
  WaiterQueueNode* cur = head;
  int len = 0;
  do {
    len++;
    cur = cur->next_;
  } while (cur != head);
  return len;
}

void WaiterQueueNode::Wait() {
  AllowGarbageCollection allow_before_parking;
  requester_->main_thread_local_heap()->BlockWhileParked([this]() {
    base::MutexGuard guard(&wait_lock_);
    while (should_wait) {
      wait_cond_var_.Wait(&wait_lock_);
    }
  });
}

// Returns false if timed out, true otherwise.
bool WaiterQueueNode::WaitFor(const base::TimeDelta& rel_time) {
  bool result;
  AllowGarbageCollection allow_before_parking;
  requester_->main_thread_local_heap()->BlockWhileParked([this, rel_time,
                                                          &result]() {
    base::MutexGuard guard(&wait_lock_);
    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeTicks timeout_time = current_time + rel_time;
    for (;;) {
      if (!should_wait) {
        result = true;
        return;
      }
      current_time = base::TimeTicks::Now();
      if (current_time >= timeout_time) {
        result = false;
        return;
      }
      base::TimeDelta time_until_timeout = timeout_time - current_time;
      bool wait_res = wait_cond_var_.WaitFor(&wait_lock_, time_until_timeout);
      USE(wait_res);
      // The wake up may have been spurious, so loop again.
    }
  });
  return result;
}

void WaiterQueueNode::Notify() {
  base::MutexGuard guard(&wait_lock_);
  should_wait = false;
  wait_cond_var_.NotifyOne();
  SetNotInListForVerification();
}

uint32_t WaiterQueueNode::NotifyAllInList() {
  WaiterQueueNode* cur = this;
  uint32_t count = 0;
  do {
    WaiterQueueNode* next = cur->next_;
    cur->Notify();
    cur = next;
    count++;
  } while (cur != this);
  return count;
}

void WaiterQueueNode::VerifyNotInList() {
  DCHECK_NULL(next_);
  DCHECK_NULL(prev_);
}

void WaiterQueueNode::SetNotInListForVerification() {
#ifdef DEBUG
  next_ = prev_ = nullptr;
#endif
}

}  // namespace detail
}  // namespace internal
}  // namespace v8
