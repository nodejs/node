// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_FOREGROUND_TASK_RUNNER_H_
#define V8_LIBPLATFORM_DEFAULT_FOREGROUND_TASK_RUNNER_H_

#include <memory>
#include <queue>

#include "include/libplatform/libplatform.h"
#include "include/v8-platform.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace platform {

class V8_PLATFORM_EXPORT DefaultForegroundTaskRunner
    : public NON_EXPORTED_BASE(TaskRunner) {
 public:
  using TimeFunction = double (*)();
  class RunTaskScope {
   public:
    explicit RunTaskScope(
        std::shared_ptr<DefaultForegroundTaskRunner> task_runner);
    ~RunTaskScope();

   private:
    RunTaskScope(const RunTaskScope&) = delete;
    RunTaskScope& operator=(const RunTaskScope&) = delete;

    std::shared_ptr<DefaultForegroundTaskRunner> task_runner_;
  };

  DefaultForegroundTaskRunner(IdleTaskSupport idle_task_support,
                              TimeFunction time_function);

  void Terminate();

  std::unique_ptr<Task> PopTaskFromQueue(MessageLoopBehavior wait_for_work);

  std::unique_ptr<IdleTask> PopTaskFromIdleQueue();

  void WaitForTaskLocked(const base::MutexGuard&);

  double MonotonicallyIncreasingTime();

  // v8::TaskRunner implementation.
  void PostTask(std::unique_ptr<Task> task) override;
  void PostDelayedTask(std::unique_ptr<Task> task,
                       double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<IdleTask> task) override;
  bool IdleTasksEnabled() override;

  void PostNonNestableTask(std::unique_ptr<Task> task) override;
  bool NonNestableTasksEnabled() const override;

 private:
  enum Nestability { kNestable, kNonNestable };

  // The same as PostTask or PostNonNestableTask, but the lock is already held
  // by the caller. The {guard} parameter should make sure that the caller is
  // holding the lock.
  void PostTaskLocked(std::unique_ptr<Task> task, Nestability nestability,
                      const base::MutexGuard&);

  // A caller of this function has to hold {lock_}. The {guard} parameter should
  // make sure that the caller is holding the lock.
  std::unique_ptr<Task> PopTaskFromDelayedQueueLocked(const base::MutexGuard&);

  // A non-nestable task is poppable only if the task runner is not nested,
  // i.e. if a task is not being run from within a task. A nestable task is
  // always poppable.
  bool HasPoppableTaskInQueue() const;

  bool terminated_ = false;
  base::Mutex lock_;
  base::ConditionVariable event_loop_control_;
  int nesting_depth_ = 0;

  using TaskQueueEntry = std::pair<Nestability, std::unique_ptr<Task>>;
  std::deque<TaskQueueEntry> task_queue_;

  IdleTaskSupport idle_task_support_;
  std::queue<std::unique_ptr<IdleTask>> idle_task_queue_;

  // Some helper constructs for the {delayed_task_queue_}.
  using DelayedEntry = std::pair<double, std::unique_ptr<Task>>;
  // Define a comparison operator for the delayed_task_queue_ to make sure
  // that the unique_ptr in the DelayedEntry is not accessed in the priority
  // queue. This is necessary because we have to reset the unique_ptr when we
  // remove a DelayedEntry from the priority queue.
  struct DelayedEntryCompare {
    bool operator()(const DelayedEntry& left, const DelayedEntry& right) const {
      return left.first > right.first;
    }
  };
  std::priority_queue<DelayedEntry, std::vector<DelayedEntry>,
                      DelayedEntryCompare>
      delayed_task_queue_;

  TimeFunction time_function_;
};

}  // namespace platform
}  // namespace v8
#endif  // V8_LIBPLATFORM_DEFAULT_FOREGROUND_TASK_RUNNER_H_
