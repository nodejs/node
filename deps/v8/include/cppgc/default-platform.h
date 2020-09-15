// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_DEFAULT_PLATFORM_H_
#define INCLUDE_CPPGC_DEFAULT_PLATFORM_H_

#include <memory>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "cppgc/platform.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

/**
 * Default task runner implementation. Keep posted tasks in a list that can be
 * processed by calling RunSingleTask() or RunUntilIdle().
 */
class V8_EXPORT DefaultTaskRunner final : public cppgc::TaskRunner {
 public:
  DefaultTaskRunner() = default;

  DefaultTaskRunner(const DefaultTaskRunner&) = delete;
  DefaultTaskRunner& operator=(const DefaultTaskRunner&) = delete;

  void PostTask(std::unique_ptr<cppgc::Task> task) override;
  void PostNonNestableTask(std::unique_ptr<cppgc::Task> task) override;
  void PostDelayedTask(std::unique_ptr<cppgc::Task> task, double) override;
  void PostNonNestableDelayedTask(std::unique_ptr<cppgc::Task> task,
                                  double) override;

  void PostIdleTask(std::unique_ptr<cppgc::IdleTask> task) override;
  bool IdleTasksEnabled() override { return true; }

  bool RunSingleTask();
  bool RunSingleIdleTask(double duration_in_seconds);

  void RunUntilIdle();

 private:
  std::vector<std::unique_ptr<cppgc::Task>> tasks_;
  std::vector<std::unique_ptr<cppgc::IdleTask>> idle_tasks_;
};

/**
 * Default platform implementation that uses std::thread for spawning job tasks.
 */
class V8_EXPORT DefaultPlatform final : public Platform {
 public:
  DefaultPlatform();
  ~DefaultPlatform() noexcept override;

  cppgc::PageAllocator* GetPageAllocator() final;

  double MonotonicallyIncreasingTime() final;

  std::shared_ptr<cppgc::TaskRunner> GetForegroundTaskRunner() final;

  std::unique_ptr<cppgc::JobHandle> PostJob(
      cppgc::TaskPriority priority,
      std::unique_ptr<cppgc::JobTask> job_task) final;

  void WaitAllForegroundTasks();
  void WaitAllBackgroundTasks();

 private:
  std::unique_ptr<PageAllocator> page_allocator_;
  std::shared_ptr<DefaultTaskRunner> foreground_task_runner_;
  std::vector<std::shared_ptr<std::thread>> job_threads_;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_DEFAULT_PLATFORM_H_
