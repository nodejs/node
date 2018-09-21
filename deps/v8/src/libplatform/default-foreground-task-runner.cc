// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-foreground-task-runner.h"

#include "src/base/platform/mutex.h"
#include "src/libplatform/default-platform.h"

namespace v8 {
namespace platform {

DefaultForegroundTaskRunner::DefaultForegroundTaskRunner(
    IdleTaskSupport idle_task_support, TimeFunction time_function)
    : idle_task_support_(idle_task_support), time_function_(time_function) {}

void DefaultForegroundTaskRunner::Terminate() {
  base::LockGuard<base::Mutex> guard(&lock_);
  terminated_ = true;

  // Drain the task queues.
  while (!task_queue_.empty()) task_queue_.pop();
  while (!delayed_task_queue_.empty()) delayed_task_queue_.pop();
  while (!idle_task_queue_.empty()) idle_task_queue_.pop();
}

void DefaultForegroundTaskRunner::PostTaskLocked(
    std::unique_ptr<Task> task, const base::LockGuard<base::Mutex>&) {
  if (terminated_) return;
  task_queue_.push(std::move(task));
  event_loop_control_.NotifyOne();
}

void DefaultForegroundTaskRunner::PostTask(std::unique_ptr<Task> task) {
  base::LockGuard<base::Mutex> guard(&lock_);
  PostTaskLocked(std::move(task), guard);
}

double DefaultForegroundTaskRunner::MonotonicallyIncreasingTime() {
  return time_function_();
}

void DefaultForegroundTaskRunner::PostDelayedTask(std::unique_ptr<Task> task,
                                                  double delay_in_seconds) {
  DCHECK_GE(delay_in_seconds, 0.0);
  base::LockGuard<base::Mutex> guard(&lock_);
  if (terminated_) return;
  double deadline = MonotonicallyIncreasingTime() + delay_in_seconds;
  delayed_task_queue_.push(std::make_pair(deadline, std::move(task)));
}

void DefaultForegroundTaskRunner::PostIdleTask(std::unique_ptr<IdleTask> task) {
  CHECK_EQ(IdleTaskSupport::kEnabled, idle_task_support_);
  base::LockGuard<base::Mutex> guard(&lock_);
  if (terminated_) return;
  idle_task_queue_.push(std::move(task));
}

bool DefaultForegroundTaskRunner::IdleTasksEnabled() {
  return idle_task_support_ == IdleTaskSupport::kEnabled;
}

std::unique_ptr<Task> DefaultForegroundTaskRunner::PopTaskFromQueue(
    MessageLoopBehavior wait_for_work) {
  base::LockGuard<base::Mutex> guard(&lock_);
  // Move delayed tasks that hit their deadline to the main queue.
  std::unique_ptr<Task> task = PopTaskFromDelayedQueueLocked(guard);
  while (task) {
    PostTaskLocked(std::move(task), guard);
    task = PopTaskFromDelayedQueueLocked(guard);
  }

  while (task_queue_.empty()) {
    if (wait_for_work == MessageLoopBehavior::kDoNotWait) return {};
    WaitForTaskLocked(guard);
  }

  task = std::move(task_queue_.front());
  task_queue_.pop();

  return task;
}

std::unique_ptr<Task>
DefaultForegroundTaskRunner::PopTaskFromDelayedQueueLocked(
    const base::LockGuard<base::Mutex>&) {
  if (delayed_task_queue_.empty()) return {};

  double now = MonotonicallyIncreasingTime();
  const DelayedEntry& deadline_and_task = delayed_task_queue_.top();
  if (deadline_and_task.first > now) return {};
  // The const_cast here is necessary because there does not exist a clean way
  // to get a unique_ptr out of the priority queue. We provide the priority
  // queue with a custom comparison operator to make sure that the priority
  // queue does not access the unique_ptr. Therefore it should be safe to reset
  // the unique_ptr in the priority queue here. Note that the DelayedEntry is
  // removed from the priority_queue immediately afterwards.
  std::unique_ptr<Task> result =
      std::move(const_cast<DelayedEntry&>(deadline_and_task).second);
  delayed_task_queue_.pop();
  return result;
}

std::unique_ptr<IdleTask> DefaultForegroundTaskRunner::PopTaskFromIdleQueue() {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (idle_task_queue_.empty()) return {};

  std::unique_ptr<IdleTask> task = std::move(idle_task_queue_.front());
  idle_task_queue_.pop();

  return task;
}

void DefaultForegroundTaskRunner::WaitForTaskLocked(
    const base::LockGuard<base::Mutex>&) {
  event_loop_control_.Wait(&lock_);
}

}  // namespace platform
}  // namespace v8
