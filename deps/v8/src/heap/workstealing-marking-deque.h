// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_WORKSTEALING_MARKING_DEQUE_
#define V8_HEAP_WORKSTEALING_MARKING_DEQUE_

#include <cstddef>

#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class HeapObject;

class StackSegment {
 public:
  static const int kNumEntries = 64;

  StackSegment(StackSegment* next, StackSegment* prev)
      : next_(next), prev_(prev), index_(0) {}

  bool Push(HeapObject* object) {
    if (IsFull()) return false;

    objects_[index_++] = object;
    return true;
  }

  bool Pop(HeapObject** object) {
    if (IsEmpty()) return false;

    *object = objects_[--index_];
    return true;
  }

  size_t Size() { return index_; }
  bool IsEmpty() { return index_ == 0; }
  bool IsFull() { return index_ == kNumEntries; }
  void Clear() { index_ = 0; }

  StackSegment* next() { return next_; }
  StackSegment* prev() { return prev_; }
  void set_next(StackSegment* next) { next_ = next; }
  void set_prev(StackSegment* prev) { prev_ = prev; }

  void Unlink() {
    if (next() != nullptr) next()->set_prev(prev());
    if (prev() != nullptr) prev()->set_next(next());
  }

 private:
  StackSegment* next_;
  StackSegment* prev_;
  size_t index_;
  HeapObject* objects_[kNumEntries];
};

class SegmentedStack {
 public:
  SegmentedStack()
      : front_(new StackSegment(nullptr, nullptr)), back_(front_) {}

  ~SegmentedStack() {
    CHECK(IsEmpty());
    delete front_;
  }

  bool Push(HeapObject* object) {
    if (!front_->Push(object)) {
      NewFront();
      bool success = front_->Push(object);
      USE(success);
      DCHECK(success);
    }
    return true;
  }

  bool Pop(HeapObject** object) {
    if (!front_->Pop(object)) {
      if (IsEmpty()) return false;
      DeleteFront();
      bool success = front_->Pop(object);
      USE(success);
      DCHECK(success);
    }
    return object;
  }

  bool IsEmpty() { return front_ == back_ && front_->IsEmpty(); }

 private:
  void NewFront() {
    StackSegment* s = new StackSegment(front_, nullptr);
    front_->set_prev(s);
    front_ = s;
  }

  void DeleteFront() { delete Unlink(front_); }

  StackSegment* Unlink(StackSegment* segment) {
    CHECK_NE(front_, back_);
    if (segment == front_) front_ = front_->next();
    if (segment == back_) back_ = back_->prev();
    segment->Unlink();
    return segment;
  }

  StackSegment* front_;
  StackSegment* back_;
};

// TODO(mlippautz): Implement actual work stealing.
class WorkStealingMarkingDeque {
 public:
  static const int kMaxNumTasks = 4;

  bool Push(int task_id, HeapObject* object) {
    DCHECK_LT(task_id, kMaxNumTasks);
    return private_stacks_[task_id].Push(object);
  }

  bool Pop(int task_id, HeapObject** object) {
    DCHECK_LT(task_id, kMaxNumTasks);
    return private_stacks_[task_id].Pop(object);
  }

  bool IsLocalEmpty(int task_id) { return private_stacks_[task_id].IsEmpty(); }

 private:
  SegmentedStack private_stacks_[kMaxNumTasks];
};

class LocalWorkStealingMarkingDeque {
 public:
  LocalWorkStealingMarkingDeque(WorkStealingMarkingDeque* deque, int task_id)
      : deque_(deque), task_id_(task_id) {}

  // Pushes an object onto the marking deque.
  bool Push(HeapObject* object) { return deque_->Push(task_id_, object); }

  // Pops an object onto the marking deque.
  bool Pop(HeapObject** object) { return deque_->Pop(task_id_, object); }

  // Returns true if the local portion of the marking deque is empty.
  bool IsEmpty() { return deque_->IsLocalEmpty(task_id_); }

  // Blocks if there are no more objects available. Returns execution with
  // |true| once new objects are available and |false| otherwise.
  bool WaitForMoreObjects() {
    // Return false once the local portion of the marking deque is drained.
    // TODO(mlippautz): Implement a barrier that can be used to synchronize
    // work stealing and emptiness.
    return !IsEmpty();
  }

 private:
  WorkStealingMarkingDeque* deque_;
  int task_id_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_WORKSTEALING_MARKING_DEQUE_
