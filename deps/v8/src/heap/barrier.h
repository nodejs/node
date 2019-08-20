// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BARRIER_H_
#define V8_HEAP_BARRIER_H_

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace internal {

// Barrier that can be used once to synchronize a dynamic number of tasks
// working concurrently.
//
// The barrier takes a timeout which is used to avoid waiting for too long. If
// any of the users ever reach the timeout they will disable the barrier and
// signal others to fall through.
//
// Usage:
//   void RunConcurrently(OneShotBarrier* shared_barrier) {
//     shared_barrier->Start();
//     do {
//       {
//         /* process work and create new work */
//         barrier->NotifyAll();
//         /* process work and create new work */
//       }
//     } while(!shared_barrier->Wait());
//   }
//
// Note: If Start() is not called in time, e.g., because the first concurrent
// task is already done processing all work, then Done() will return true
// immediately.
class OneshotBarrier {
 public:
  explicit OneshotBarrier(base::TimeDelta timeout) : timeout_(timeout) {}

  void Start() {
    base::MutexGuard guard(&mutex_);
    tasks_++;
  }

  void NotifyAll() {
    base::MutexGuard guard(&mutex_);
    if (waiting_ > 0) condition_.NotifyAll();
  }

  bool Wait() {
    base::MutexGuard guard(&mutex_);
    if (done_) return true;

    DCHECK_LE(waiting_, tasks_);
    waiting_++;
    if (waiting_ == tasks_) {
      done_ = true;
      condition_.NotifyAll();
    } else {
      // Spurious wakeup is ok here.
      if (!condition_.WaitFor(&mutex_, timeout_)) {
        // If predefined timeout was reached, Stop waiting and signal being done
        // also to other tasks.
        done_ = true;
      }
    }
    waiting_--;
    return done_;
  }

  // Only valid to be called in a sequential setting.
  bool DoneForTesting() const { return done_; }

 private:
  base::ConditionVariable condition_;
  base::Mutex mutex_;
  base::TimeDelta timeout_;
  int tasks_ = 0;
  int waiting_ = 0;
  bool done_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_BARRIER_H_
