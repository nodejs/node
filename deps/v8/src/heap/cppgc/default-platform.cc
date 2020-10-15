// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/default-platform.h"

#include <chrono>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "src/base/page-allocator.h"

namespace cppgc {

namespace {

// Simple implementation of JobTask based on std::thread.
class DefaultJobHandle : public JobHandle {
 public:
  explicit DefaultJobHandle(std::shared_ptr<std::thread> thread)
      : thread_(std::move(thread)) {}

  void NotifyConcurrencyIncrease() override {}
  void Join() override {
    if (thread_->joinable()) thread_->join();
  }
  void Cancel() override { Join(); }
  bool IsRunning() override { return thread_->joinable(); }

 private:
  std::shared_ptr<std::thread> thread_;
};

}  // namespace

void DefaultTaskRunner::PostTask(std::unique_ptr<cppgc::Task> task) {
  tasks_.push_back(std::move(task));
}

void DefaultTaskRunner::PostNonNestableTask(std::unique_ptr<cppgc::Task> task) {
  PostTask(std::move(task));
}

void DefaultTaskRunner::PostDelayedTask(std::unique_ptr<cppgc::Task> task,
                                        double) {
  PostTask(std::move(task));
}

void DefaultTaskRunner::PostNonNestableDelayedTask(
    std::unique_ptr<cppgc::Task> task, double) {
  PostTask(std::move(task));
}

void DefaultTaskRunner::PostIdleTask(std::unique_ptr<cppgc::IdleTask> task) {
  idle_tasks_.push_back(std::move(task));
}

bool DefaultTaskRunner::RunSingleTask() {
  if (!tasks_.size()) return false;

  tasks_.back()->Run();
  tasks_.pop_back();

  return true;
}

bool DefaultTaskRunner::RunSingleIdleTask(double deadline_in_seconds) {
  if (!idle_tasks_.size()) return false;

  idle_tasks_.back()->Run(deadline_in_seconds);
  idle_tasks_.pop_back();

  return true;
}

void DefaultTaskRunner::RunUntilIdle() {
  for (auto& task : tasks_) {
    task->Run();
  }
  tasks_.clear();

  for (auto& task : idle_tasks_) {
    task->Run(std::numeric_limits<double>::infinity());
  }
  idle_tasks_.clear();
}

DefaultPlatform::DefaultPlatform()
    : page_allocator_(std::make_unique<v8::base::PageAllocator>()),
      foreground_task_runner_(std::make_shared<DefaultTaskRunner>()) {}

DefaultPlatform::~DefaultPlatform() noexcept { WaitAllBackgroundTasks(); }

cppgc::PageAllocator* DefaultPlatform::GetPageAllocator() {
  return page_allocator_.get();
}

double DefaultPlatform::MonotonicallyIncreasingTime() {
  return std::chrono::duration<double>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

std::shared_ptr<cppgc::TaskRunner> DefaultPlatform::GetForegroundTaskRunner() {
  return foreground_task_runner_;
}

std::unique_ptr<cppgc::JobHandle> DefaultPlatform::PostJob(
    cppgc::TaskPriority priority, std::unique_ptr<cppgc::JobTask> job_task) {
  auto thread = std::make_shared<std::thread>([task = std::move(job_task)] {
    class SimpleDelegate final : public cppgc::JobDelegate {
     public:
      bool ShouldYield() override { return false; }
      void NotifyConcurrencyIncrease() override {}
    } delegate;

    if (task) task->Run(&delegate);
  });
  job_threads_.push_back(thread);
  return std::make_unique<DefaultJobHandle>(std::move(thread));
}

void DefaultPlatform::WaitAllForegroundTasks() {
  foreground_task_runner_->RunUntilIdle();
}

void DefaultPlatform::WaitAllBackgroundTasks() {
  for (auto& thread : job_threads_) {
    thread->join();
  }
  job_threads_.clear();
}

}  // namespace cppgc
