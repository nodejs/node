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
  base::MutexGuard guard(&lock_);
  DCHECK(terminated_);
  DCHECK(task_queue_.empty());
}

double DelayedTaskQueue::MonotonicallyIncreasingTime() {
  return time_function_();
}

void DelayedTaskQueue::Append(std::unique_ptr<Task> task) {
  base::MutexGuard guard(&lock_);
  DCHECK(!terminated_);
  task_queue_.push(std::move(task));
  queues_condition_var_.NotifyOne();
}

void DelayedTaskQueue::AppendDelayed(std::unique_ptr<Task> task,
                                     double delay_in_seconds) {
  DCHECK_GE(delay_in_seconds, 0.0);
  double deadline = MonotonicallyIncreasingTime() + delay_in_seconds;
  {
    base::MutexGuard guard(&lock_);
    DCHECK(!terminated_);
    delayed_task_queue_.emplace(deadline, std::move(task));
    queues_condition_var_.NotifyOne();
  }
}

std::unique_ptr<Task> DelayedTaskQueue::GetNext() {
  base::MutexGuard guard(&lock_);
  for (;;) {
    // Move delayed tasks that have hit their deadline to the main queue.
    double now = MonotonicallyIncreasingTime();
    std::unique_ptr<Task> task = PopTaskFromDelayedQueue(now);
    while (task) {
      task_queue_.push(std::move(task));
      task = PopTaskFromDelayedQueue(now);
    }
    if (!task_queue_.empty()) {
      std::unique_ptr<Task> result = std::move(task_queue_.front());
      task_queue_.pop();
      return result;
    }

    if (terminated_) {
      queues_condition_var_.NotifyAll();
      return nullptr;
    }

    if (task_queue_.empty() && !delayed_task_queue_.empty()) {
      // Wait for the next delayed task or a newly posted task.
      double wait_in_seconds = delayed_task_queue_.begin()->first - now;
      base::TimeDelta wait_delta = base::TimeDelta::FromMicroseconds(
          base::TimeConstants::kMicrosecondsPerSecond * wait_in_seconds);

      // WaitFor unfortunately doesn't care about our fake time and will wait
      // the 'real' amount of time, based on whatever clock the system call
      // uses.
      bool notified = queues_condition_var_.WaitFor(&lock_, wait_delta);
      USE(notified);
    } else {
      queues_condition_var_.Wait(&lock_);
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
  base::MutexGuard guard(&lock_);
  DCHECK(!terminated_);
  terminated_ = true;
  queues_condition_var_.NotifyAll();
}

}  // namespace platform
}  // namespace v8
