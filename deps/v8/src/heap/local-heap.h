// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_H_
#define V8_HEAP_LOCAL_HEAP_H_

#include <atomic>

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace internal {

class Heap;

class LocalHeap {
 public:
  V8_EXPORT_PRIVATE explicit LocalHeap(Heap* heap);
  V8_EXPORT_PRIVATE ~LocalHeap();

  // Invoked by main thread to signal this thread that it needs to halt in a
  // safepoint.
  void RequestSafepoint();

  // Frequently invoked by local thread to check whether safepoint was requested
  // from the main thread.
  void Safepoint();

 private:
  enum class ThreadState {
    // Threads in this state need to be stopped in a safepoint.
    Running,
    // Thread was parked, which means that the thread is not allowed to access
    // or manipulate the heap in any way.
    Parked,
    // Thread was stopped in a safepoint.
    Safepoint
  };

  void Park();
  void Unpark();
  bool IsSafepointRequested();
  void ClearSafepointRequested();
  void EnterSafepoint();

  Heap* heap_;
  base::Mutex state_mutex_;
  base::ConditionVariable state_condvar_;
  ThreadState state_;
  std::atomic<bool> safepoint_requested_;
  LocalHeap* prev_;
  LocalHeap* next_;

  friend class Heap;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_HEAP_H_
