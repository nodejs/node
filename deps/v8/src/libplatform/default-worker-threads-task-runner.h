// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_WORKER_THREADS_TASK_RUNNER_H_
#define V8_LIBPLATFORM_DEFAULT_WORKER_THREADS_TASK_RUNNER_H_

#include <vector>

#include "include/libplatform/libplatform-export.h"
#include "include/v8-platform.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/libplatform/delayed-task-queue.h"

namespace v8 {
namespace platform {

class V8_PLATFORM_EXPORT DefaultWorkerThreadsTaskRunner
    : public NON_EXPORTED_BASE(TaskRunner) {
 public:
  using TimeFunction = double (*)();

  DefaultWorkerThreadsTaskRunner(uint32_t thread_pool_size,
                                 TimeFunction time_function);

  ~DefaultWorkerThreadsTaskRunner() override;

  void Terminate();

  double MonotonicallyIncreasingTime();

  // v8::TaskRunner implementation.
  void PostTask(std::unique_ptr<Task> task) override;

  void PostDelayedTask(std::unique_ptr<Task> task,
                       double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<IdleTask> task) override;

  bool IdleTasksEnabled() override;

 private:
  class WorkerThread : public base::Thread {
   public:
    explicit WorkerThread(DefaultWorkerThreadsTaskRunner* runner);
    ~WorkerThread() override;

    // This thread attempts to get tasks in a loop from |runner_| and run them.
    void Run() override;

   private:
    DefaultWorkerThreadsTaskRunner* runner_;

    DISALLOW_COPY_AND_ASSIGN(WorkerThread);
  };

  // Called by the WorkerThread. Gets the next take (delayed or immediate) to be
  // executed. Blocks if no task is available.
  std::unique_ptr<Task> GetNext();

  bool terminated_ = false;
  base::Mutex lock_;
  DelayedTaskQueue queue_;
  std::vector<std::unique_ptr<WorkerThread>> thread_pool_;
  TimeFunction time_function_;
};

}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_DEFAULT_WORKER_THREADS_TASK_RUNNER_H_
