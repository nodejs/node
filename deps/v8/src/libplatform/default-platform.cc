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
#include "src/libplatform/tracing/trace-buffer.h"
#include "src/libplatform/tracing/trace-writer.h"
#include "src/libplatform/worker-thread.h"

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

v8::Platform* CreateDefaultPlatform(int thread_pool_size,
                                    IdleTaskSupport idle_task_support,
                                    InProcessStackDumping in_process_stack_dumping,
                                    v8::TracingController* tracing_controller) {
  if (in_process_stack_dumping == InProcessStackDumping::kEnabled) {
    v8::base::debug::EnableInProcessStackDumping();
  }
  DefaultPlatform* platform =
    new DefaultPlatform(idle_task_support, tracing_controller);
  platform->SetThreadPoolSize(thread_pool_size);
  platform->EnsureInitialized();
  return platform;
}

bool PumpMessageLoop(v8::Platform* platform, v8::Isolate* isolate,
                     MessageLoopBehavior behavior) {
  return static_cast<DefaultPlatform*>(platform)->PumpMessageLoop(isolate,
                                                                  behavior);
}

void EnsureEventLoopInitialized(v8::Platform* platform, v8::Isolate* isolate) {
  return static_cast<DefaultPlatform*>(platform)->EnsureEventLoopInitialized(
      isolate);
}

void RunIdleTasks(v8::Platform* platform, v8::Isolate* isolate,
                  double idle_time_in_seconds) {
  static_cast<DefaultPlatform*>(platform)->RunIdleTasks(isolate,
                                                        idle_time_in_seconds);
}

void SetTracingController(
    v8::Platform* platform,
    v8::platform::tracing::TracingController* tracing_controller) {
  return static_cast<DefaultPlatform*>(platform)->SetTracingController(
      tracing_controller);
}

const int DefaultPlatform::kMaxThreadPoolSize = 8;

DefaultPlatform::DefaultPlatform(IdleTaskSupport idle_task_support,
                                 v8::TracingController* tracing_controller)
    : initialized_(false),
      thread_pool_size_(0),
      idle_task_support_(idle_task_support) {
  if (tracing_controller) {
    tracing_controller_.reset(tracing_controller);
  } else {
    tracing::TraceWriter* writer = new tracing::NullTraceWriter();
    tracing::TraceBuffer* ring_buffer =
        new tracing::TraceBufferRingBuffer(1, writer);
    tracing::TracingController* controller = new tracing::TracingController();
    controller->Initialize(ring_buffer);
    tracing_controller_.reset(controller);
  }
}

DefaultPlatform::~DefaultPlatform() {
  base::LockGuard<base::Mutex> guard(&lock_);
  queue_.Terminate();
  if (initialized_) {
    for (auto i = thread_pool_.begin(); i != thread_pool_.end(); ++i) {
      delete *i;
    }
  }
  for (auto i = main_thread_queue_.begin(); i != main_thread_queue_.end();
       ++i) {
    while (!i->second.empty()) {
      delete i->second.front();
      i->second.pop();
    }
  }
  for (auto i = main_thread_delayed_queue_.begin();
       i != main_thread_delayed_queue_.end(); ++i) {
    while (!i->second.empty()) {
      delete i->second.top().second;
      i->second.pop();
    }
  }
  for (auto& i : main_thread_idle_queue_) {
    while (!i.second.empty()) {
      delete i.second.front();
      i.second.pop();
    }
  }
}


void DefaultPlatform::SetThreadPoolSize(int thread_pool_size) {
  base::LockGuard<base::Mutex> guard(&lock_);
  DCHECK(thread_pool_size >= 0);
  if (thread_pool_size < 1) {
    thread_pool_size = base::SysInfo::NumberOfProcessors() - 1;
  }
  thread_pool_size_ =
      std::max(std::min(thread_pool_size, kMaxThreadPoolSize), 1);
}


void DefaultPlatform::EnsureInitialized() {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (initialized_) return;
  initialized_ = true;

  for (int i = 0; i < thread_pool_size_; ++i)
    thread_pool_.push_back(new WorkerThread(&queue_));
}


Task* DefaultPlatform::PopTaskInMainThreadQueue(v8::Isolate* isolate) {
  auto it = main_thread_queue_.find(isolate);
  if (it == main_thread_queue_.end() || it->second.empty()) {
    return NULL;
  }
  Task* task = it->second.front();
  it->second.pop();
  return task;
}


Task* DefaultPlatform::PopTaskInMainThreadDelayedQueue(v8::Isolate* isolate) {
  auto it = main_thread_delayed_queue_.find(isolate);
  if (it == main_thread_delayed_queue_.end() || it->second.empty()) {
    return NULL;
  }
  double now = MonotonicallyIncreasingTime();
  std::pair<double, Task*> deadline_and_task = it->second.top();
  if (deadline_and_task.first > now) {
    return NULL;
  }
  it->second.pop();
  return deadline_and_task.second;
}

IdleTask* DefaultPlatform::PopTaskInMainThreadIdleQueue(v8::Isolate* isolate) {
  auto it = main_thread_idle_queue_.find(isolate);
  if (it == main_thread_idle_queue_.end() || it->second.empty()) {
    return nullptr;
  }
  IdleTask* task = it->second.front();
  it->second.pop();
  return task;
}

void DefaultPlatform::EnsureEventLoopInitialized(v8::Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (event_loop_control_.count(isolate) == 0) {
    event_loop_control_.insert(std::make_pair(
        isolate, std::unique_ptr<base::Semaphore>(new base::Semaphore(0))));
  }
}

void DefaultPlatform::WaitForForegroundWork(v8::Isolate* isolate) {
  base::Semaphore* semaphore = nullptr;
  {
    base::LockGuard<base::Mutex> guard(&lock_);
    DCHECK_EQ(event_loop_control_.count(isolate), 1);
    semaphore = event_loop_control_[isolate].get();
  }
  DCHECK_NOT_NULL(semaphore);
  semaphore->Wait();
}

bool DefaultPlatform::PumpMessageLoop(v8::Isolate* isolate,
                                      MessageLoopBehavior behavior) {
  if (behavior == MessageLoopBehavior::kWaitForWork) {
    WaitForForegroundWork(isolate);
  }
  Task* task = NULL;
  {
    base::LockGuard<base::Mutex> guard(&lock_);

    // Move delayed tasks that hit their deadline to the main queue.
    task = PopTaskInMainThreadDelayedQueue(isolate);
    while (task != NULL) {
      ScheduleOnForegroundThread(isolate, task);
      task = PopTaskInMainThreadDelayedQueue(isolate);
    }

    task = PopTaskInMainThreadQueue(isolate);

    if (task == NULL) {
      return behavior == MessageLoopBehavior::kWaitForWork;
    }
  }
  task->Run();
  delete task;
  return true;
}

void DefaultPlatform::RunIdleTasks(v8::Isolate* isolate,
                                   double idle_time_in_seconds) {
  DCHECK(IdleTaskSupport::kEnabled == idle_task_support_);
  double deadline_in_seconds =
      MonotonicallyIncreasingTime() + idle_time_in_seconds;
  while (deadline_in_seconds > MonotonicallyIncreasingTime()) {
    {
      IdleTask* task;
      {
        base::LockGuard<base::Mutex> guard(&lock_);
        task = PopTaskInMainThreadIdleQueue(isolate);
      }
      if (task == nullptr) return;
      task->Run(deadline_in_seconds);
      delete task;
    }
  }
}

void DefaultPlatform::CallOnBackgroundThread(Task* task,
                                             ExpectedRuntime expected_runtime) {
  EnsureInitialized();
  queue_.Append(task);
}

void DefaultPlatform::ScheduleOnForegroundThread(v8::Isolate* isolate,
                                                 Task* task) {
  main_thread_queue_[isolate].push(task);
  if (event_loop_control_.count(isolate) != 0) {
    event_loop_control_[isolate]->Signal();
  }
}

void DefaultPlatform::CallOnForegroundThread(v8::Isolate* isolate, Task* task) {
  base::LockGuard<base::Mutex> guard(&lock_);
  ScheduleOnForegroundThread(isolate, task);
}


void DefaultPlatform::CallDelayedOnForegroundThread(Isolate* isolate,
                                                    Task* task,
                                                    double delay_in_seconds) {
  base::LockGuard<base::Mutex> guard(&lock_);
  double deadline = MonotonicallyIncreasingTime() + delay_in_seconds;
  main_thread_delayed_queue_[isolate].push(std::make_pair(deadline, task));
}

void DefaultPlatform::CallIdleOnForegroundThread(Isolate* isolate,
                                                 IdleTask* task) {
  base::LockGuard<base::Mutex> guard(&lock_);
  main_thread_idle_queue_[isolate].push(task);
}

bool DefaultPlatform::IdleTasksEnabled(Isolate* isolate) {
  return idle_task_support_ == IdleTaskSupport::kEnabled;
}

double DefaultPlatform::MonotonicallyIncreasingTime() {
  return base::TimeTicks::HighResolutionNow().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

TracingController* DefaultPlatform::GetTracingController() {
  return tracing_controller_.get();
}

void DefaultPlatform::SetTracingController(
    v8::TracingController* tracing_controller) {
  DCHECK_NOT_NULL(tracing_controller);
  tracing_controller_.reset(tracing_controller);
}

size_t DefaultPlatform::NumberOfAvailableBackgroundThreads() {
  return static_cast<size_t>(thread_pool_size_);
}

Platform::StackTracePrinter DefaultPlatform::GetStackTracePrinter() {
  return PrintStackTrace;
}

}  // namespace platform
}  // namespace v8
