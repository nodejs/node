// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/delayed-task-queue.h"

#include "include/v8-platform.h"
#include "src/base/logging.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace platform {

DelayedTaskQueue::DelayedTaskQueue(TimeFunction time_function)
    : time_function_(time_function) {}

DelayedTaskQueue::~DelayedTaskQueue() {
  DCHECK(terminated_);
  DCHECK(task_queue_.empty());
}

double DelayedTaskQueue::MonotonicallyIncreasingTime() {
  return time_function_();
}

void DelayedTaskQueue::Append(std::unique_ptr<Task> task) {
  DCHECK(!terminated_);
  task_queue_.push(std::move(task));
}

void DelayedTaskQueue::AppendDelayed(std::unique_ptr<Task> task,
                                     double delay_in_seconds) {
  DCHECK_GE(delay_in_seconds, 0.0);
  double deadline = MonotonicallyIncreasingTime() + delay_in_seconds;
  {
    DCHECK(!terminated_);
    delayed_task_queue_.emplace(deadline, std::move(task));
  }
}

DelayedTaskQueue::MaybeNextTask DelayedTaskQueue::TryGetNext() {
  for (;;) {
    // Move delayed tasks that have hit their deadline to the main queue.
    double now = MonotonicallyIncreasingTime();
    for (;;) {
      std::unique_ptr<Task> task = PopTaskFromDelayedQueue(now);
      if (!task) break;
      task_queue_.push(std::move(task));
    }
    if (!task_queue_.empty()) {
      std::unique_ptr<Task> task = std::move(task_queue_.front());
      task_queue_.pop();
      return {MaybeNextTask::kTask, std::move(task), {}};
    }

    if (terminated_) {
      return {MaybeNextTask::kTerminated, {}, {}};
    }

    if (task_queue_.empty() && !delayed_task_queue_.empty()) {
      // Wait for the next delayed task or a newly posted task.
      double wait_in_seconds = delayed_task_queue_.begin()->first - now;
      return {
          MaybeNextTask::kWaitDelayed,
          {},
          base::TimeDelta::FromMicroseconds(
              base::TimeConstants::kMicrosecondsPerSecond * wait_in_seconds)};
    } else {
      return {MaybeNextTask::kWaitIndefinite, {}, {}};
    }
  }
}

// Gets the next task from the delayed queue for which the deadline has passed
// according to |now|. Returns nullptr if no such task exists.
std::unique_ptr<Task> DelayedTaskQueue::PopTaskFromDelayedQueue(double now) {
  if (delayed_task_queue_.empty()) return nullptr;

  auto it = delayed_task_queue_.begin();
  if (it->first > now) return nullptr;

  std::unique_ptr<Task> result = std::move(it->second);
  delayed_task_queue_.erase(it);
  return result;
}

void DelayedTaskQueue::Terminate() {
  DCHECK(!terminated_);
  terminated_ = true;
}

}  // namespace platform
}  // namespace v8
