// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
#define V8_LIBPLATFORM_DEFAULT_PLATFORM_H_

#include <map>
#include <memory>

#include "include/libplatform/libplatform-export.h"
#include "include/libplatform/libplatform.h"
#include "include/libplatform/v8-tracing.h"
#include "include/v8-platform.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace platform {

class Thread;
class WorkerThread;
class DefaultForegroundTaskRunner;
class DefaultWorkerThreadsTaskRunner;
class DefaultPageAllocator;

class V8_PLATFORM_EXPORT DefaultPlatform : public NON_EXPORTED_BASE(Platform) {
 public:
  explicit DefaultPlatform(
      int thread_pool_size = 0,
      IdleTaskSupport idle_task_support = IdleTaskSupport::kDisabled,
      std::unique_ptr<v8::TracingController> tracing_controller = {});

  ~DefaultPlatform() override;

  DefaultPlatform(const DefaultPlatform&) = delete;
  DefaultPlatform& operator=(const DefaultPlatform&) = delete;

  void EnsureBackgroundTaskRunnerInitialized();

  bool PumpMessageLoop(
      v8::Isolate* isolate,
      MessageLoopBehavior behavior = MessageLoopBehavior::kDoNotWait);

  void RunIdleTasks(v8::Isolate* isolate, double idle_time_in_seconds);

  void SetTracingController(
      std::unique_ptr<v8::TracingController> tracing_controller);

  using TimeFunction = double (*)();

  void SetTimeFunctionForTesting(TimeFunction time_function);

  // v8::Platform implementation.
  int NumberOfWorkerThreads() override;
  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override;
  void CallOnWorkerThread(std::unique_ptr<Task> task) override;
  void CallDelayedOnWorkerThread(std::unique_ptr<Task> task,
                                 double delay_in_seconds) override;
  bool IdleTasksEnabled(Isolate* isolate) override;
  std::unique_ptr<JobHandle> PostJob(
      TaskPriority priority, std::unique_ptr<JobTask> job_state) override;
  double MonotonicallyIncreasingTime() override;
  double CurrentClockTimeMillis() override;
  v8::TracingController* GetTracingController() override;
  StackTracePrinter GetStackTracePrinter() override;
  v8::PageAllocator* GetPageAllocator() override;

  void NotifyIsolateShutdown(Isolate* isolate);

 private:
  base::Mutex lock_;
  const int thread_pool_size_;
  IdleTaskSupport idle_task_support_;
  std::shared_ptr<DefaultWorkerThreadsTaskRunner> worker_threads_task_runner_;
  std::map<v8::Isolate*, std::shared_ptr<DefaultForegroundTaskRunner>>
      foreground_task_runner_map_;

  std::unique_ptr<TracingController> tracing_controller_;
  std::unique_ptr<PageAllocator> page_allocator_;

  TimeFunction time_function_for_testing_ = nullptr;
};

}  // namespace platform
}  // namespace v8


#endif  // V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
