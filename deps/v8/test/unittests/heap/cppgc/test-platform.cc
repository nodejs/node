// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/cppgc/test-platform.h"

#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/heap/cppgc/default-job.h"

namespace cppgc {
namespace internal {
namespace testing {

namespace {
class TestJobThread final : public v8::base::Thread {
 public:
  using id = uint8_t;

  explicit TestJobThread(TestJob* job) : Thread(Options("job")), job_(job) {}

  void Run() final;

  static size_t GetMaxSupportedConcurrency() { return 4u; }

 private:
  TestJob* const job_;
};
}  // namespace

// Default implementation of Jobs based on std::thread.
class TestJob final : public DefaultJobImpl<TestJobThread> {
 public:
  explicit TestJob(Key key, std::unique_ptr<cppgc::JobTask> job_task)
      : DefaultJobImpl(key, std::move(job_task)) {}

  std::shared_ptr<TestJobThread> CreateThread(DefaultJobImpl* job) final {
    std::shared_ptr<TestJobThread> thread =
        std::make_shared<TestJobThread>(this);
    const bool thread_started = thread->Start();
    USE(thread_started);
    DCHECK(thread_started);
    return thread;
  }
};

void TestJobThread::Run() {
  DCHECK_NOT_NULL(job_);
  job_->RunJobTask();
}

void TestTaskRunner::PostTask(std::unique_ptr<cppgc::Task> task) {
  tasks_.push_back(std::move(task));
}

void TestTaskRunner::PostNonNestableTask(std::unique_ptr<cppgc::Task> task) {
  PostTask(std::move(task));
}

void TestTaskRunner::PostDelayedTask(std::unique_ptr<cppgc::Task> task,
                                     double) {
  PostTask(std::move(task));
}

void TestTaskRunner::PostNonNestableDelayedTask(
    std::unique_ptr<cppgc::Task> task, double) {
  PostTask(std::move(task));
}

void TestTaskRunner::PostIdleTask(std::unique_ptr<cppgc::IdleTask> task) {
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

TestPlatform::TestPlatform()
    : foreground_task_runner_(std::make_unique<TestTaskRunner>()) {}

TestPlatform::~TestPlatform() V8_NOEXCEPT { WaitAllBackgroundTasks(); }

std::unique_ptr<cppgc::JobHandle> TestPlatform::PostJob(
    cppgc::TaskPriority, std::unique_ptr<cppgc::JobTask> job_task) {
  if (AreBackgroundTasksDisabled()) return {};

  std::shared_ptr<TestJob> job =
      DefaultJobFactory<TestJob>::Create(std::move(job_task));
  jobs_.push_back(job);
  return std::make_unique<TestJob::JobHandle>(std::move(job));
}

double TestPlatform::MonotonicallyIncreasingTime() {
  return v8::base::TimeTicks::HighResolutionNow().ToInternalValue() /
         static_cast<double>(v8::base::Time::kMicrosecondsPerSecond);
}

void TestPlatform::WaitAllForegroundTasks() {
  foreground_task_runner_->RunUntilIdle();
}

void TestPlatform::WaitAllBackgroundTasks() {
  for (auto& job : jobs_) {
    job->Join();
  }
  jobs_.clear();
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
