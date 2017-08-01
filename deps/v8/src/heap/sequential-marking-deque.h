// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SEQUENTIAL_MARKING_DEQUE_
#define V8_HEAP_SEQUENTIAL_MARKING_DEQUE_

#include <deque>

#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/cancelable-task.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;
class HeapObject;

// ----------------------------------------------------------------------------
// Marking deque for tracing live objects.
class SequentialMarkingDeque {
 public:
  explicit SequentialMarkingDeque(Heap* heap)
      : backing_store_(nullptr),
        backing_store_committed_size_(0),
        array_(nullptr),
        top_(0),
        bottom_(0),
        mask_(0),
        overflowed_(false),
        in_use_(false),
        uncommit_task_pending_(false),
        heap_(heap) {}

  void SetUp();
  void TearDown();

  // Ensures that the marking deque is committed and will stay committed until
  // StopUsing() is called.
  void StartUsing();
  void StopUsing();
  void Clear();

  inline bool IsFull() { return ((top_ + 1) & mask_) == bottom_; }

  inline bool IsEmpty() { return top_ == bottom_; }

  int Size() {
    // Return (top - bottom + capacity) % capacity, where capacity = mask + 1.
    return (top_ - bottom_ + mask_ + 1) & mask_;
  }

  bool overflowed() const { return overflowed_; }

  void ClearOverflowed() { overflowed_ = false; }

  void SetOverflowed() { overflowed_ = true; }

  // Push the object on the marking stack if there is room, otherwise mark the
  // deque as overflowed and wait for a rescan of the heap.
  INLINE(bool Push(HeapObject* object)) {
    if (IsFull()) {
      SetOverflowed();
      return false;
    } else {
      array_[top_] = object;
      top_ = ((top_ + 1) & mask_);
      return true;
    }
  }

  INLINE(HeapObject* Pop()) {
    DCHECK(!IsEmpty());
    top_ = ((top_ - 1) & mask_);
    HeapObject* object = array_[top_];
    return object;
  }

  // Unshift the object into the marking stack if there is room, otherwise mark
  // the deque as overflowed and wait for a rescan of the heap.
  INLINE(bool Unshift(HeapObject* object)) {
    if (IsFull()) {
      SetOverflowed();
      return false;
    } else {
      bottom_ = ((bottom_ - 1) & mask_);
      array_[bottom_] = object;
      return true;
    }
  }

  // Calls the specified callback on each element of the deque and replaces
  // the element with the result of the callback. If the callback returns
  // nullptr then the element is removed from the deque.
  // The callback must accept HeapObject* and return HeapObject*.
  template <typename Callback>
  void Update(Callback callback) {
    int i = bottom_;
    int new_top = bottom_;
    while (i != top_) {
      HeapObject* object = callback(array_[i]);
      if (object) {
        array_[new_top] = object;
        new_top = (new_top + 1) & mask_;
      }
      i = (i + 1) & mask_;
    }
    top_ = new_top;
  }

 private:
  // This task uncommits the marking_deque backing store if
  // markin_deque->in_use_ is false.
  class UncommitTask : public CancelableTask {
   public:
    explicit UncommitTask(Isolate* isolate,
                          SequentialMarkingDeque* marking_deque)
        : CancelableTask(isolate), marking_deque_(marking_deque) {}

   private:
    // CancelableTask override.
    void RunInternal() override {
      base::LockGuard<base::Mutex> guard(&marking_deque_->mutex_);
      if (!marking_deque_->in_use_) {
        marking_deque_->Uncommit();
      }
      marking_deque_->uncommit_task_pending_ = false;
    }

    SequentialMarkingDeque* marking_deque_;
    DISALLOW_COPY_AND_ASSIGN(UncommitTask);
  };

  static const size_t kMaxSize = 4 * MB;
  static const size_t kMinSize = 256 * KB;

  // Must be called with mutex lock.
  void EnsureCommitted();

  // Must be called with mutex lock.
  void Uncommit();

  // Must be called with mutex lock.
  void StartUncommitTask();

  base::Mutex mutex_;

  base::VirtualMemory* backing_store_;
  size_t backing_store_committed_size_;
  HeapObject** array_;
  // array_[(top - 1) & mask_] is the top element in the deque.  The Deque is
  // empty when top_ == bottom_.  It is full when top_ + 1 == bottom
  // (mod mask + 1).
  int top_;
  int bottom_;
  int mask_;
  bool overflowed_;
  // in_use_ == true after taking mutex lock implies that the marking deque is
  // committed and will stay committed at least until in_use_ == false.
  bool in_use_;
  bool uncommit_task_pending_;
  Heap* heap_;

  DISALLOW_COPY_AND_ASSIGN(SequentialMarkingDeque);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SEQUENTIAL_MARKING_DEQUE_
