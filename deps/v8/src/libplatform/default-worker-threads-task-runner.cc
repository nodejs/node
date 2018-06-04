// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-worker-threads-task-runner.h"

#include "src/base/platform/mutex.h"
#include "src/libplatform/worker-thread.h"

namespace v8 {
namespace platform {

DefaultWorkerThreadsTaskRunner::DefaultWorkerThreadsTaskRunner(
    uint32_t thread_pool_size) {
  for (uint32_t i = 0; i < thread_pool_size; ++i) {
    thread_pool_.push_back(base::make_unique<WorkerThread>(&queue_));
  }
}

DefaultWorkerThreadsTaskRunner::~DefaultWorkerThreadsTaskRunner() {
  // This destructor is needed because we have unique_ptr to the WorkerThreads,
  // und the {WorkerThread} class is forward declared in the header file.
}

void DefaultWorkerThreadsTaskRunner::Terminate() {
  base::LockGuard<base::Mutex> guard(&lock_);
  terminated_ = true;
  queue_.Terminate();
  // Clearing the thread pool lets all worker threads join.
  thread_pool_.clear();
}

void DefaultWorkerThreadsTaskRunner::PostTask(std::unique_ptr<Task> task) {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (terminated_) return;
  queue_.Append(std::move(task));
}

void DefaultWorkerThreadsTaskRunner::PostDelayedTask(std::unique_ptr<Task> task,
                                                     double delay_in_seconds) {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (terminated_) return;
  // There is no use case for this function on a worker thread at the
  // moment, but it is still part of the interface.
  UNIMPLEMENTED();
}

void DefaultWorkerThreadsTaskRunner::PostIdleTask(
    std::unique_ptr<IdleTask> task) {
  // There are no idle worker tasks.
  UNREACHABLE();
}

bool DefaultWorkerThreadsTaskRunner::IdleTasksEnabled() {
  // There are no idle worker tasks.
  return false;
}

}  // namespace platform
}  // namespace v8
