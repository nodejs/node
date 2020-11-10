// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-platform.h"

#include <algorithm>
#include <queue>

#include "include/libplatform/libplatform.h"
#include "src/base/debug/stack_trace.h"
#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/sys-info.h"
#include "src/libplatform/default-foreground-task-runner.h"
#include "src/libplatform/default-job.h"
#include "src/libplatform/default-worker-threads-task-runner.h"

namespace v8 {
namespace platform {

namespace {

void PrintStackTrace() {
  v8::base::debug::StackTrace trace;
  trace.Print();
  // Avoid dumping duplicate stack trace on abort signal.
  v8::base::debug::DisableSignalStackDump();
}

}  // namespace

std::unique_ptr<v8::Platform> NewDefaultPlatform(
    int thread_pool_size, IdleTaskSupport idle_task_support,
    InProcessStackDumping in_process_stack_dumping,
    std::unique_ptr<v8::TracingController> tracing_controller) {
  if (in_process_stack_dumping == InProcessStackDumping::kEnabled) {
    v8::base::debug::EnableInProcessStackDumping();
  }
  auto platform = std::make_unique<DefaultPlatform>(
      thread_pool_size, idle_task_support, std::move(tracing_controller));
  platform->EnsureBackgroundTaskRunnerInitialized();
  return platform;
}

bool PumpMessageLoop(v8::Platform* platform, v8::Isolate* isolate,
                     MessageLoopBehavior behavior) {
  return static_cast<DefaultPlatform*>(platform)->PumpMessageLoop(isolate,
                                                                  behavior);
}

void RunIdleTasks(v8::Platform* platform, v8::Isolate* isolate,
                  double idle_time_in_seconds) {
  static_cast<DefaultPlatform*>(platform)->RunIdleTasks(isolate,
                                                        idle_time_in_seconds);
}

void SetTracingController(
    v8::Platform* platform,
    v8::platform::tracing::TracingController* tracing_controller) {
  static_cast<DefaultPlatform*>(platform)->SetTracingController(
      std::unique_ptr<v8::TracingController>(tracing_controller));
}

namespace {
constexpr int kMaxThreadPoolSize = 16;

int GetActualThreadPoolSize(int thread_pool_size) {
  DCHECK_GE(thread_pool_size, 0);
  if (thread_pool_size < 1) {
    thread_pool_size = base::SysInfo::NumberOfProcessors() - 1;
  }
  return std::max(std::min(thread_pool_size, kMaxThreadPoolSize), 1);
}
}  // namespace

DefaultPlatform::DefaultPlatform(
    int thread_pool_size, IdleTaskSupport idle_task_support,
    std::unique_ptr<v8::TracingController> tracing_controller)
    : thread_pool_size_(GetActualThreadPoolSize(thread_pool_size)),
      idle_task_support_(idle_task_support),
      tracing_controller_(std::move(tracing_controller)),
      page_allocator_(std::make_unique<v8::base::PageAllocator>()) {
  if (!tracing_controller_) {
    tracing::TracingController* controller = new tracing::TracingController();
#if !defined(V8_USE_PERFETTO)
    controller->Initialize(nullptr);
#endif
    tracing_controller_.reset(controller);
  }
}

DefaultPlatform::~DefaultPlatform() {
  base::MutexGuard guard(&lock_);
  if (worker_threads_task_runner_) worker_threads_task_runner_->Terminate();
  for (const auto& it : foreground_task_runner_map_) {
    it.second->Terminate();
  }
}

namespace {

double DefaultTimeFunction() {
  return base::TimeTicks::HighResolutionNow().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

}  // namespace

void DefaultPlatform::EnsureBackgroundTaskRunnerInitialized() {
  base::MutexGuard guard(&lock_);
  if (!worker_threads_task_runner_) {
    worker_threads_task_runner_ =
        std::make_shared<DefaultWorkerThreadsTaskRunner>(
            thread_pool_size_, time_function_for_testing_
                                   ? time_function_for_testing_
                                   : DefaultTimeFunction);
  }
}

void DefaultPlatform::SetTimeFunctionForTesting(
    DefaultPlatform::TimeFunction time_function) {
  base::MutexGuard guard(&lock_);
  time_function_for_testing_ = time_function;
  // The time function has to be right after the construction of the platform.
  DCHECK(foreground_task_runner_map_.empty());
}

bool DefaultPlatform::PumpMessageLoop(v8::Isolate* isolate,
                                      MessageLoopBehavior wait_for_work) {
  bool failed_result = wait_for_work == MessageLoopBehavior::kWaitForWork;
  std::shared_ptr<DefaultForegroundTaskRunner> task_runner;
  {
    base::MutexGuard guard(&lock_);
    auto it = foreground_task_runner_map_.find(isolate);
    if (it == foreground_task_runner_map_.end()) return failed_result;
    task_runner = it->second;
  }

  std::unique_ptr<Task> task = task_runner->PopTaskFromQueue(wait_for_work);
  if (!task) return failed_result;

  DefaultForegroundTaskRunner::RunTaskScope scope(task_runner);
  task->Run();
  return true;
}

void DefaultPlatform::RunIdleTasks(v8::Isolate* isolate,
                                   double idle_time_in_seconds) {
  DCHECK_EQ(IdleTaskSupport::kEnabled, idle_task_support_);
  std::shared_ptr<DefaultForegroundTaskRunner> task_runner;
  {
    base::MutexGuard guard(&lock_);
    if (foreground_task_runner_map_.find(isolate) ==
        foreground_task_runner_map_.end()) {
      return;
    }
    task_runner = foreground_task_runner_map_[isolate];
  }
  double deadline_in_seconds =
      MonotonicallyIncreasingTime() + idle_time_in_seconds;

  while (deadline_in_seconds > MonotonicallyIncreasingTime()) {
    std::unique_ptr<IdleTask> task = task_runner->PopTaskFromIdleQueue();
    if (!task) return;
    DefaultForegroundTaskRunner::RunTaskScope scope(task_runner);
    task->Run(deadline_in_seconds);
  }
}

std::shared_ptr<TaskRunner> DefaultPlatform::GetForegroundTaskRunner(
    v8::Isolate* isolate) {
  base::MutexGuard guard(&lock_);
  if (foreground_task_runner_map_.find(isolate) ==
      foreground_task_runner_map_.end()) {
    foreground_task_runner_map_.insert(std::make_pair(
        isolate, std::make_shared<DefaultForegroundTaskRunner>(
                     idle_task_support_, time_function_for_testing_
                                             ? time_function_for_testing_
                                             : DefaultTimeFunction)));
  }
  return foreground_task_runner_map_[isolate];
}

void DefaultPlatform::CallOnWorkerThread(std::unique_ptr<Task> task) {
  EnsureBackgroundTaskRunnerInitialized();
  worker_threads_task_runner_->PostTask(std::move(task));
}

void DefaultPlatform::CallDelayedOnWorkerThread(std::unique_ptr<Task> task,
                                                double delay_in_seconds) {
  EnsureBackgroundTaskRunnerInitialized();
  worker_threads_task_runner_->PostDelayedTask(std::move(task),
                                               delay_in_seconds);
}

bool DefaultPlatform::IdleTasksEnabled(Isolate* isolate) {
  return idle_task_support_ == IdleTaskSupport::kEnabled;
}

std::unique_ptr<JobHandle> DefaultPlatform::PostJob(
    TaskPriority priority, std::unique_ptr<JobTask> job_task) {
  size_t num_worker_threads = 0;
  switch (priority) {
    case TaskPriority::kUserBlocking:
      num_worker_threads = NumberOfWorkerThreads();
      break;
    case TaskPriority::kUserVisible:
      num_worker_threads = NumberOfWorkerThreads() / 2;
      break;
    case TaskPriority::kBestEffort:
      num_worker_threads = 1;
      break;
  }
  return std::make_unique<DefaultJobHandle>(std::make_shared<DefaultJobState>(
      this, std::move(job_task), priority, num_worker_threads));
}

double DefaultPlatform::MonotonicallyIncreasingTime() {
  if (time_function_for_testing_) return time_function_for_testing_();
  return DefaultTimeFunction();
}

double DefaultPlatform::CurrentClockTimeMillis() {
  return base::OS::TimeCurrentMillis();
}

TracingController* DefaultPlatform::GetTracingController() {
  return tracing_controller_.get();
}

void DefaultPlatform::SetTracingController(
    std::unique_ptr<v8::TracingController> tracing_controller) {
  DCHECK_NOT_NULL(tracing_controller.get());
  tracing_controller_ = std::move(tracing_controller);
}

int DefaultPlatform::NumberOfWorkerThreads() { return thread_pool_size_; }

Platform::StackTracePrinter DefaultPlatform::GetStackTracePrinter() {
  return PrintStackTrace;
}

v8::PageAllocator* DefaultPlatform::GetPageAllocator() {
  return page_allocator_.get();
}

}  // namespace platform
}  // namespace v8
