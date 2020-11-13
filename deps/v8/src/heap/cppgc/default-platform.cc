// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/default-platform.h"

#include <chrono>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "src/base/sys-info.h"
#include "src/heap/cppgc/default-job.h"

namespace cppgc {

namespace internal {

// Default implementation of Jobs based on std::thread.
namespace {
class DefaultJobThread final : private std::thread {
 public:
  template <typename Function>
  explicit DefaultJobThread(Function function)
      : std::thread(std::move(function)) {}
  ~DefaultJobThread() { DCHECK(!joinable()); }

  void Join() { join(); }

  static size_t GetMaxSupportedConcurrency() {
    return v8::base::SysInfo::NumberOfProcessors() - 1;
  }
};
}  // namespace

class DefaultJob final : public DefaultJobImpl<DefaultJobThread> {
 public:
  DefaultJob(Key key, std::unique_ptr<cppgc::JobTask> job_task)
      : DefaultJobImpl(key, std::move(job_task)) {}

  std::shared_ptr<DefaultJobThread> CreateThread(DefaultJobImpl* job) final {
    return std::make_shared<DefaultJobThread>([job = this] {
      DCHECK_NOT_NULL(job);
      job->RunJobTask();
    });
  }
};

}  // namespace internal

void DefaultTaskRunner::PostTask(std::unique_ptr<cppgc::Task> task) {
  tasks_.push_back(std::move(task));
}

void DefaultTaskRunner::PostDelayedTask(std::unique_ptr<cppgc::Task> task,
                                        double) {
  PostTask(std::move(task));
}

void DefaultTaskRunner::PostNonNestableTask(std::unique_ptr<cppgc::Task>) {
  UNREACHABLE();
}

void DefaultTaskRunner::PostNonNestableDelayedTask(std::unique_ptr<cppgc::Task>,
                                                   double) {
  UNREACHABLE();
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
  std::shared_ptr<internal::DefaultJob> job =
      internal::DefaultJobFactory<internal::DefaultJob>::Create(
          std::move(job_task));
  jobs_.push_back(job);
  return std::make_unique<internal::DefaultJob::JobHandle>(std::move(job));
}

void DefaultPlatform::WaitAllForegroundTasks() {
  foreground_task_runner_->RunUntilIdle();
}

void DefaultPlatform::WaitAllBackgroundTasks() {
  for (auto& job : jobs_) {
    job->Join();
  }
  jobs_.clear();
}

}  // namespace cppgc
