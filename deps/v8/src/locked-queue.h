// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOCKED_QUEUE_H_
#define V8_LOCKED_QUEUE_H_

#include "src/allocation.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {

// Simple lock-based unbounded size queue (multi producer; multi consumer) based
// on "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue
// Algorithms" by M. Scott and M. Michael.
// See:
// https://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html
template <typename Record>
class LockedQueue final BASE_EMBEDDED {
 public:
  inline LockedQueue();
  inline ~LockedQueue();
  inline void Enqueue(const Record& record);
  inline bool Dequeue(Record* record);
  inline bool IsEmpty() const;
  inline bool Peek(Record* record) const;

 private:
  struct Node;

  mutable base::Mutex head_mutex_;
  base::Mutex tail_mutex_;
  Node* head_;
  Node* tail_;

  DISALLOW_COPY_AND_ASSIGN(LockedQueue);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_LOCKED_QUEUE_H_
