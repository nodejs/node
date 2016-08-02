// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOCKED_QUEUE_INL_
#define V8_LOCKED_QUEUE_INL_

#include "src/base/atomic-utils.h"
#include "src/locked-queue.h"

namespace v8 {
namespace internal {

template <typename Record>
struct LockedQueue<Record>::Node : Malloced {
  Node() : next(nullptr) {}
  Record value;
  base::AtomicValue<Node*> next;
};


template <typename Record>
inline LockedQueue<Record>::LockedQueue() {
  head_ = new Node();
  CHECK(head_ != nullptr);
  tail_ = head_;
}


template <typename Record>
inline LockedQueue<Record>::~LockedQueue() {
  // Destroy all remaining nodes. Note that we do not destroy the actual values.
  Node* old_node = nullptr;
  Node* cur_node = head_;
  while (cur_node != nullptr) {
    old_node = cur_node;
    cur_node = cur_node->next.Value();
    delete old_node;
  }
}


template <typename Record>
inline void LockedQueue<Record>::Enqueue(const Record& record) {
  Node* n = new Node();
  CHECK(n != nullptr);
  n->value = record;
  {
    base::LockGuard<base::Mutex> guard(&tail_mutex_);
    tail_->next.SetValue(n);
    tail_ = n;
  }
}


template <typename Record>
inline bool LockedQueue<Record>::Dequeue(Record* record) {
  Node* old_head = nullptr;
  {
    base::LockGuard<base::Mutex> guard(&head_mutex_);
    old_head = head_;
    Node* const next_node = head_->next.Value();
    if (next_node == nullptr) return false;
    *record = next_node->value;
    head_ = next_node;
  }
  delete old_head;
  return true;
}


template <typename Record>
inline bool LockedQueue<Record>::IsEmpty() const {
  base::LockGuard<base::Mutex> guard(&head_mutex_);
  return head_->next.Value() == nullptr;
}


template <typename Record>
inline bool LockedQueue<Record>::Peek(Record* record) const {
  base::LockGuard<base::Mutex> guard(&head_mutex_);
  Node* const next_node = head_->next.Value();
  if (next_node == nullptr) return false;
  *record = next_node->value;
  return true;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOCKED_QUEUE_INL_
