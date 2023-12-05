// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DELAYED_TASK_QUEUE_H_
#define V8_LIBPLATFORM_DELAYED_TASK_QUEUE_H_

#include <map>
#include <memory>
#include <queue>

#include "include/libplatform/libplatform-export.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

namespace v8 {

class Task;

namespace platform {

// DelayedTaskQueue provides queueing for immediate and delayed tasks. It does
// not provide any guarantees about ordering of tasks, except that immediate
// tasks will be run in the order that they are posted.
//
// This class is not thread-safe, and should be guarded by a lock.
class V8_PLATFORM_EXPORT DelayedTaskQueue {
 public:
  using TimeFunction = double (*)();

  explicit DelayedTaskQueue(TimeFunction time_function);
  ~DelayedTaskQueue();

  DelayedTaskQueue(const DelayedTaskQueue&) = delete;
  DelayedTaskQueue& operator=(const DelayedTaskQueue&) = delete;

  double MonotonicallyIncreasingTime();

  // Appends an immediate task to the queue. The queue takes ownership of
  // |task|. Tasks appended via this method will be run in order.
  void Append(std::unique_ptr<Task> task);

  // Appends a delayed task to the queue. There is no ordering guarantee
  // provided regarding delayed tasks, both with respect to other delayed tasks
  // and non-delayed tasks that were appended using Append().
  void AppendDelayed(std::unique_ptr<Task> task, double delay_in_seconds);

  struct MaybeNextTask {
    enum { kTask, kWaitIndefinite, kWaitDelayed, kTerminated } state;
    std::unique_ptr<Task> task;
    base::TimeDelta wait_time;
  };
  // Returns the next task to process, or the amount of time to wait until the
  // next delayed task.  Returns nullptr if the queue is terminated. Will return
  // either an immediate task posted using Append() or a delayed task where the
  // deadline has passed, according to the |time_function| provided in the
  // constructor.
  MaybeNextTask TryGetNext();

  // Terminate the queue.
  void Terminate();

 private:
  std::unique_ptr<Task> PopTaskFromDelayedQueue(double now);

  std::queue<std::unique_ptr<Task>> task_queue_;
  std::multimap<double, std::unique_ptr<Task>> delayed_task_queue_;
  bool terminated_ = false;
  TimeFunction time_function_;
};

}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_DELAYED_TASK_QUEUE_H_
