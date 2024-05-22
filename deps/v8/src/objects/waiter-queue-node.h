// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_WAITER_QUEUE_NODE_H_
#define V8_OBJECTS_WAITER_QUEUE_NODE_H_

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

namespace v8 {

namespace base {
class TimeDelta;
}  // namespace base

namespace internal {

class Isolate;

namespace detail {

// To manage waiting threads inside JSSynchronizationPrimitives, there is a
// process-wide doubly-linked intrusive list per waiter (i.e. mutex or condition
// variable). There is a per-thread node allocated on the stack when the thread
// goes to sleep during waiting.
//
// When compressing pointers (including when sandboxing), the access to the
// on-stack node is indirected through the shared external pointer table.
//
// TODO(v8:12547): Unittest this.
class V8_NODISCARD WaiterQueueNode final {
 public:
  explicit WaiterQueueNode(Isolate* requester);

  ~WaiterQueueNode();

  // Enqueues {new_tail}, mutating {head} to be the new head.
  static void Enqueue(WaiterQueueNode** head, WaiterQueueNode* new_tail);

  using DequeueMatcher = std::function<bool(WaiterQueueNode*)>;
  // Dequeues the first waiter for which {matcher} returns true and returns it;
  // mutating {head} to be the new head.
  //
  // The queue lock must be held in the synchronization primitive that owns
  // this waiter queue when calling this method.
  static WaiterQueueNode* DequeueMatching(WaiterQueueNode** head,
                                          const DequeueMatcher& matcher);

  static WaiterQueueNode* Dequeue(WaiterQueueNode** head);

  // Splits at most {count} nodes of the waiter list of into its own list and
  // returns it, mutating {head} to be the head of the back list.
  static WaiterQueueNode* Split(WaiterQueueNode** head, uint32_t count);

  // This method must be called from a known waiter queue head. Incorrectly
  // encoded lists can cause this method to infinitely loop.
  static int LengthFromHead(WaiterQueueNode* head);

  void Wait();

  // Returns false if timed out, true otherwise.
  bool WaitFor(const base::TimeDelta& rel_time);

  void Notify();

  uint32_t NotifyAllInList();

  bool should_wait = false;

 private:
  void VerifyNotInList();

  void SetNotInListForVerification();

  Isolate* requester_;

  // The queue wraps around, e.g. the head's prev is the tail, and the tail's
  // next is the head.
  WaiterQueueNode* next_ = nullptr;
  WaiterQueueNode* prev_ = nullptr;

  base::Mutex wait_lock_;
  base::ConditionVariable wait_cond_var_;
};

}  // namespace detail
}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_WAITER_QUEUE_NODE_H_
