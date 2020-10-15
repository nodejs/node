// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/cppgc/test-platform.h"

#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"

namespace cppgc {
namespace internal {
namespace testing {

void TestTaskRunner::PostTask(std::unique_ptr<v8::Task> task) {
  tasks_.push_back(std::move(task));
}

void TestTaskRunner::PostNonNestableTask(std::unique_ptr<v8::Task> task) {
  PostTask(std::move(task));
}

void TestTaskRunner::PostDelayedTask(std::unique_ptr<v8::Task> task, double) {
  PostTask(std::move(task));
}

void TestTaskRunner::PostNonNestableDelayedTask(std::unique_ptr<v8::Task> task,
                                                double) {
  PostTask(std::move(task));
}

void TestTaskRunner::PostIdleTask(std::unique_ptr<v8::IdleTask> task) {
  idle_tasks_.push_back(std::move(task));
}

bool TestTaskRunner::RunSingleTask() {
  if (!tasks_.size()) return false;

  tasks_.back()->Run();
  tasks_.pop_back();

  return true;
}

bool TestTaskRunner::RunSingleIdleTask(double deadline_in_seconds) {
  if (!idle_tasks_.size()) return false;

  idle_tasks_.back()->Run(deadline_in_seconds);
  idle_tasks_.pop_back();

  return true;
}

void TestTaskRunner::RunUntilIdle() {
  for (auto& task : tasks_) {
    task->Run();
  }
  tasks_.clear();

  for (auto& task : idle_tasks_) {
    task->Run(std::numeric_limits<double>::infinity());
  }
  idle_tasks_.clear();
}

class TestPlatform::TestJobHandle : public v8::JobHandle {
 public:
  explicit TestJobHandle(const std::shared_ptr<JobThread>& thread)
      : thread_(thread) {
    const bool success = thread_->Start();
    USE(success);
  }

  void NotifyConcurrencyIncrease() override {}
  void Join() override { thread_->Join(); }
  void Cancel() override { Join(); }
  bool IsRunning() override { return true; }

 private:
  std::shared_ptr<JobThread> thread_;
};

TestPlatform::TestPlatform()
    : foreground_task_runner_(std::make_unique<TestTaskRunner>()) {}

TestPlatform::~TestPlatform() V8_NOEXCEPT { WaitAllBackgroundTasks(); }

std::unique_ptr<v8::JobHandle> TestPlatform::PostJob(
    v8::TaskPriority, std::unique_ptr<v8::JobTask> job_task) {
  if (AreBackgroundTasksDisabled()) return {};

  auto thread = std::make_shared<JobThread>(std::move(job_task));
  job_threads_.push_back(thread);
  return std::make_unique<TestJobHandle>(std::move(thread));
}

double TestPlatform::MonotonicallyIncreasingTime() {
  return v8::base::TimeTicks::HighResolutionNow().ToInternalValue() /
         static_cast<double>(v8::base::Time::kMicrosecondsPerSecond);
}

void TestPlatform::WaitAllForegroundTasks() {
  foreground_task_runner_->RunUntilIdle();
}

void TestPlatform::WaitAllBackgroundTasks() {
  for (auto& thread : job_threads_) {
    thread->Join();
  }
  job_threads_.clear();
}

TestPlatform::DisableBackgroundTasksScope::DisableBackgroundTasksScope(
    TestPlatform* platform)
    : platform_(platform) {
  ++platform_->disabled_background_tasks_;
}

TestPlatform::DisableBackgroundTasksScope::~DisableBackgroundTasksScope()
    V8_NOEXCEPT {
  --platform_->disabled_background_tasks_;
}

}  // namespace testing
}  // namespace internal
}  // namespace cppgc
