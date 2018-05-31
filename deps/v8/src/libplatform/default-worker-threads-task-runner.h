// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_WORKER_THREADS_TASK_RUNNER_H_
#define V8_LIBPLATFORM_DEFAULT_WORKER_THREADS_TASK_RUNNER_H_

#include "include/v8-platform.h"
#include "src/libplatform/task-queue.h"

namespace v8 {
namespace platform {

class Thread;
class WorkerThread;

class V8_PLATFORM_EXPORT DefaultWorkerThreadsTaskRunner
    : public NON_EXPORTED_BASE(TaskRunner) {
 public:
  DefaultWorkerThreadsTaskRunner(uint32_t thread_pool_size);

  ~DefaultWorkerThreadsTaskRunner();

  void Terminate();

  // v8::TaskRunner implementation.
  void PostTask(std::unique_ptr<Task> task) override;

  void PostDelayedTask(std::unique_ptr<Task> task,
                       double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<IdleTask> task) override;

  bool IdleTasksEnabled() override;

 private:
  bool terminated_ = false;
  base::Mutex lock_;
  TaskQueue queue_;
  std::vector<std::unique_ptr<WorkerThread>> thread_pool_;
};

}  // namespace platform
}  // namespace v8
#endif  // V8_LIBPLATFORM_DEFAULT_WORKER_THREADS_TASK_RUNNER_H_
