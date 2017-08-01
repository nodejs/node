// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_MARKING_DEQUE_
#define V8_HEAP_CONCURRENT_MARKING_DEQUE_

#include <deque>

#include "src/base/platform/mutex.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;
class HeapObject;

enum class MarkingThread { kMain, kConcurrent };

enum class TargetDeque { kShared, kBailout };

// The concurrent marking deque supports deque operations for two threads:
// main and concurrent. It is implemented using two deques: shared and bailout.
//
// The concurrent thread can use the push and pop operations with the
// MarkingThread::kConcurrent argument. All other operations are intended
// to be used by the main thread only.
//
// The interface of the concurrent marking deque for the main thread matches
// that of the sequential marking deque, so they can be easily switched
// at compile time without updating the main thread call-sites.
//
// The shared deque is shared between the main thread and the concurrent
// thread, so both threads can push to and pop from the shared deque.
// The bailout deque stores objects that cannot be processed by the concurrent
// thread. Only the concurrent thread can push to it and only the main thread
// can pop from it.
class ConcurrentMarkingDeque {
 public:
  // The heap parameter is needed to match the interface
  // of the sequential marking deque.
  explicit ConcurrentMarkingDeque(Heap* heap) {}

  // Pushes the object into the specified deque assuming that the function is
  // called on the specified thread. The main thread can push only to the shared
  // deque. The concurrent thread can push to both deques.
  bool Push(HeapObject* object, MarkingThread thread = MarkingThread::kMain,
            TargetDeque target = TargetDeque::kShared) {
    switch (target) {
      case TargetDeque::kShared:
        shared_deque_.Push(object);
        break;
      case TargetDeque::kBailout:
        bailout_deque_.Push(object);
        break;
    }
    return true;
  }

  // Pops an object from the bailout or shared deque assuming that the function
  // is called on the specified thread. The main thread first tries to pop the
  // bailout deque. If the deque is empty then it tries the shared deque.
  // If the shared deque is also empty, then the function returns nullptr.
  // The concurrent thread pops only from the shared deque.
  HeapObject* Pop(MarkingThread thread = MarkingThread::kMain) {
    if (thread == MarkingThread::kMain) {
      HeapObject* result = bailout_deque_.Pop();
      if (result != nullptr) return result;
    }
    return shared_deque_.Pop();
  }

  // All the following operations can used only by the main thread.
  void Clear() {
    bailout_deque_.Clear();
    shared_deque_.Clear();
  }

  bool IsFull() { return false; }

  bool IsEmpty() { return bailout_deque_.IsEmpty() && shared_deque_.IsEmpty(); }

  int Size() { return bailout_deque_.Size() + shared_deque_.Size(); }

  // This is used for a large array with a progress bar.
  // For simpicity, unshift to the bailout deque so that the concurrent thread
  // does not see such objects.
  bool Unshift(HeapObject* object) {
    bailout_deque_.Unshift(object);
    return true;
  }

  // Calls the specified callback on each element of the deques and replaces
  // the element with the result of the callback. If the callback returns
  // nullptr then the element is removed from the deque.
  // The callback must accept HeapObject* and return HeapObject*.
  template <typename Callback>
  void Update(Callback callback) {
    bailout_deque_.Update(callback);
    shared_deque_.Update(callback);
  }

  // These empty functions are needed to match the interface
  // of the sequential marking deque.
  void SetUp() {}
  void TearDown() {}
  void StartUsing() {}
  void StopUsing() {}
  void ClearOverflowed() {}
  void SetOverflowed() {}
  bool overflowed() const { return false; }

 private:
  // Simple, slow, and thread-safe deque that forwards all operations to
  // a lock-protected std::deque.
  class Deque {
   public:
    Deque() { cache_padding_[0] = 0; }
    void Clear() {
      base::LockGuard<base::Mutex> guard(&mutex_);
      return deque_.clear();
    }
    bool IsEmpty() {
      base::LockGuard<base::Mutex> guard(&mutex_);
      return deque_.empty();
    }
    int Size() {
      base::LockGuard<base::Mutex> guard(&mutex_);
      return static_cast<int>(deque_.size());
    }
    void Push(HeapObject* object) {
      base::LockGuard<base::Mutex> guard(&mutex_);
      deque_.push_back(object);
    }
    HeapObject* Pop() {
      base::LockGuard<base::Mutex> guard(&mutex_);
      if (deque_.empty()) return nullptr;
      HeapObject* result = deque_.back();
      deque_.pop_back();
      return result;
    }
    void Unshift(HeapObject* object) {
      base::LockGuard<base::Mutex> guard(&mutex_);
      deque_.push_front(object);
    }
    template <typename Callback>
    void Update(Callback callback) {
      base::LockGuard<base::Mutex> guard(&mutex_);
      std::deque<HeapObject*> new_deque;
      for (auto object : deque_) {
        HeapObject* new_object = callback(object);
        if (new_object) {
          new_deque.push_back(new_object);
        }
      }
      deque_.swap(new_deque);
    }

   private:
    base::Mutex mutex_;
    std::deque<HeapObject*> deque_;
    // Ensure that two deques do not share the same cache line.
    static int const kCachePadding = 64;
    char cache_padding_[kCachePadding];
  };
  Deque bailout_deque_;
  Deque shared_deque_;
  DISALLOW_COPY_AND_ASSIGN(ConcurrentMarkingDeque);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CONCURRENT_MARKING_DEQUE_
