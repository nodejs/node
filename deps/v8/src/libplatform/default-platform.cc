// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-platform.h"

#include <algorithm>
#include <queue>

#include "include/libplatform/libplatform.h"
#include "src/base/debug/stack_trace.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/sys-info.h"
#include "src/libplatform/default-background-task-runner.h"
#include "src/libplatform/default-foreground-task-runner.h"

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
  std::unique_ptr<DefaultPlatform> platform(
      new DefaultPlatform(idle_task_support, std::move(tracing_controller)));
  platform->SetThreadPoolSize(thread_pool_size);
  platform->EnsureBackgroundTaskRunnerInitialized();
  return std::move(platform);
}

v8::Platform* CreateDefaultPlatform(
    int thread_pool_size, IdleTaskSupport idle_task_support,
    InProcessStackDumping in_process_stack_dumping,
    v8::TracingController* tracing_controller) {
  return NewDefaultPlatform(
             thread_pool_size, idle_task_support, in_process_stack_dumping,
             std::unique_ptr<v8::TracingController>(tracing_controller))
      .release();
}

bool PumpMessageLoop(v8::Platform* platform, v8::Isolate* isolate,
                     MessageLoopBehavior behavior) {
  return static_cast<DefaultPlatform*>(platform)->PumpMessageLoop(isolate,
                                                                  behavior);
}

void EnsureEventLoopInitialized(v8::Platform* platform, v8::Isolate* isolate) {
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

const int DefaultPlatform::kMaxThreadPoolSize = 8;

DefaultPlatform::DefaultPlatform(
    IdleTaskSupport idle_task_support,
    std::unique_ptr<v8::TracingController> tracing_controller)
    : thread_pool_size_(0),
      idle_task_support_(idle_task_support),
      tracing_controller_(std::move(tracing_controller)),
      time_function_for_testing_(nullptr) {
  if (!tracing_controller_) {
    tracing::TracingController* controller = new tracing::TracingController();
    controller->Initialize(nullptr);
    tracing_controller_.reset(controller);
  }
}

DefaultPlatform::~DefaultPlatform() {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (background_task_runner_) background_task_runner_->Terminate();
  for (auto it : foreground_task_runner_map_) {
    it.second->Terminate();
  }
}

void DefaultPlatform::SetThreadPoolSize(int thread_pool_size) {
  base::LockGuard<base::Mutex> guard(&lock_);
  DCHECK_GE(thread_pool_size, 0);
  if (thread_pool_size < 1) {
    thread_pool_size = base::SysInfo::NumberOfProcessors() - 1;
  }
  thread_pool_size_ =
      std::max(std::min(thread_pool_size, kMaxThreadPoolSize), 1);
}

void DefaultPlatform::EnsureBackgroundTaskRunnerInitialized() {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (!background_task_runner_) {
    background_task_runner_ =
        std::make_shared<DefaultBackgroundTaskRunner>(thread_pool_size_);
  }
}

namespace {

double DefaultTimeFunction() {
  return base::TimeTicks::HighResolutionNow().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

}  // namespace

void DefaultPlatform::SetTimeFunctionForTesting(
    DefaultPlatform::TimeFunction time_function) {
  base::LockGuard<base::Mutex> guard(&lock_);
  time_function_for_testing_ = time_function;
  // The time function has to be right after the construction of the platform.
  DCHECK(foreground_task_runner_map_.empty());
}

bool DefaultPlatform::PumpMessageLoop(v8::Isolate* isolate,
                                      MessageLoopBehavior behavior) {
  bool failed_result = behavior == MessageLoopBehavior::kWaitForWork;
  std::shared_ptr<DefaultForegroundTaskRunner> task_runner;
  {
    base::LockGuard<base::Mutex> guard(&lock_);
    if (foreground_task_runner_map_.find(isolate) ==
        foreground_task_runner_map_.end()) {
      return failed_result;
    }
    task_runner = foreground_task_runner_map_[isolate];
  }
  if (behavior == MessageLoopBehavior::kWaitForWork) {
    task_runner->WaitForTask();
  }

  std::unique_ptr<Task> task = task_runner->PopTaskFromQueue();
  if (!task) return failed_result;

  task->Run();
  return true;
}

void DefaultPlatform::RunIdleTasks(v8::Isolate* isolate,
                                   double idle_time_in_seconds) {
  DCHECK_EQ(IdleTaskSupport::kEnabled, idle_task_support_);
  std::shared_ptr<DefaultForegroundTaskRunner> task_runner;
  {
    base::LockGuard<base::Mutex> guard(&lock_);
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
    task->Run(deadline_in_seconds);
  }
}

std::shared_ptr<TaskRunner> DefaultPlatform::GetForegroundTaskRunner(
    v8::Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(&lock_);
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

std::shared_ptr<TaskRunner> DefaultPlatform::GetBackgroundTaskRunner(
    v8::Isolate*) {
  EnsureBackgroundTaskRunnerInitialized();
  return background_task_runner_;
}

void DefaultPlatform::CallOnBackgroundThread(Task* task,
                                             ExpectedRuntime expected_runtime) {
  GetBackgroundTaskRunner(nullptr)->PostTask(std::unique_ptr<Task>(task));
}

void DefaultPlatform::CallOnForegroundThread(v8::Isolate* isolate, Task* task) {
  GetForegroundTaskRunner(isolate)->PostTask(std::unique_ptr<Task>(task));
}


void DefaultPlatform::CallDelayedOnForegroundThread(Isolate* isolate,
                                                    Task* task,
                                                    double delay_in_seconds) {
  GetForegroundTaskRunner(isolate)->PostDelayedTask(std::unique_ptr<Task>(task),
                                                    delay_in_seconds);
}

void DefaultPlatform::CallIdleOnForegroundThread(Isolate* isolate,
                                                 IdleTask* task) {
  GetForegroundTaskRunner(isolate)->PostIdleTask(
      std::unique_ptr<IdleTask>(task));
}

bool DefaultPlatform::IdleTasksEnabled(Isolate* isolate) {
  return idle_task_support_ == IdleTaskSupport::kEnabled;
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

size_t DefaultPlatform::NumberOfAvailableBackgroundThreads() {
  return static_cast<size_t>(thread_pool_size_);
}

Platform::StackTracePrinter DefaultPlatform::GetStackTracePrinter() {
  return PrintStackTrace;
}

}  // namespace platform
}  // namespace v8
