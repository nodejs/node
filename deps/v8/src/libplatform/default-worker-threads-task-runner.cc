// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-worker-threads-task-runner.h"

#include "src/libplatform/delayed-task-queue.h"

namespace v8 {
namespace platform {

DefaultWorkerThreadsTaskRunner::DefaultWorkerThreadsTaskRunner(
    uint32_t thread_pool_size, TimeFunction time_function)
    : queue_(time_function),
      time_function_(time_function),
      thread_pool_size_(thread_pool_size) {
  for (uint32_t i = 0; i < thread_pool_size; ++i) {
    thread_pool_.push_back(std::make_unique<WorkerThread>(this));
  }
}

DefaultWorkerThreadsTaskRunner::~DefaultWorkerThreadsTaskRunner() = default;

double DefaultWorkerThreadsTaskRunner::MonotonicallyIncreasingTime() {
  return time_function_();
}

bool DefaultWorkerThreadsTaskRunner::RunsTasksOnCurrentThread() const {
  USE(thread_pool_size_);
  DCHECK_EQ(thread_pool_size_, 1);
  return single_worker_thread_id_.load(std::memory_order_relaxed) ==
         base::OS::GetCurrentThreadId();
}

void DefaultWorkerThreadsTaskRunner::Terminate() {
  base::MutexGuard guard(&lock_);
  terminated_ = true;
  queue_.Terminate();
  // Clearing the thread pool lets all worker threads join.
  thread_pool_.clear();
  single_worker_thread_id_.store(0, std::memory_order_relaxed);
}

void DefaultWorkerThreadsTaskRunner::PostTask(std::unique_ptr<Task> task) {
  base::MutexGuard guard(&lock_);
  if (terminated_) return;
  queue_.Append(std::move(task));
}

void DefaultWorkerThreadsTaskRunner::PostDelayedTask(std::unique_ptr<Task> task,
                                                     double delay_in_seconds) {
  base::MutexGuard guard(&lock_);
  if (terminated_) return;
  queue_.AppendDelayed(std::move(task), delay_in_seconds);
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

std::unique_ptr<Task> DefaultWorkerThreadsTaskRunner::GetNext() {
  return queue_.GetNext();
}

DefaultWorkerThreadsTaskRunner::WorkerThread::WorkerThread(
    DefaultWorkerThreadsTaskRunner* runner)
    : Thread(Options("V8 DefaultWorkerThreadsTaskRunner WorkerThread")),
      runner_(runner) {
  CHECK(Start());
}

DefaultWorkerThreadsTaskRunner::WorkerThread::~WorkerThread() { Join(); }

void DefaultWorkerThreadsTaskRunner::WorkerThread::Run() {
  runner_->single_worker_thread_id_.store(base::OS::GetCurrentThreadId(),
                                          std::memory_order_relaxed);
  while (std::unique_ptr<Task> task = runner_->GetNext()) {
    task->Run();
  }
}

}  // namespace platform
}  // namespace v8
