// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
#define V8_LIBPLATFORM_DEFAULT_PLATFORM_H_

#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <vector>

#include "include/libplatform/libplatform-export.h"
#include "include/libplatform/libplatform.h"
#include "include/libplatform/v8-tracing.h"
#include "include/v8-platform.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/libplatform/task-queue.h"

namespace v8 {
namespace platform {

class TaskQueue;
class Thread;
class WorkerThread;

class V8_PLATFORM_EXPORT DefaultPlatform : public NON_EXPORTED_BASE(Platform) {
 public:
  explicit DefaultPlatform(
      IdleTaskSupport idle_task_support = IdleTaskSupport::kDisabled,
      v8::TracingController* tracing_controller = nullptr);
  virtual ~DefaultPlatform();

  void SetThreadPoolSize(int thread_pool_size);

  void EnsureInitialized();

  bool PumpMessageLoop(
      v8::Isolate* isolate,
      MessageLoopBehavior behavior = MessageLoopBehavior::kDoNotWait);
  void EnsureEventLoopInitialized(v8::Isolate* isolate);

  void RunIdleTasks(v8::Isolate* isolate, double idle_time_in_seconds);

  void SetTracingController(v8::TracingController* tracing_controller);

  // v8::Platform implementation.
  size_t NumberOfAvailableBackgroundThreads() override;
  void CallOnBackgroundThread(Task* task,
                              ExpectedRuntime expected_runtime) override;
  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override;
  void CallDelayedOnForegroundThread(Isolate* isolate, Task* task,
                                     double delay_in_seconds) override;
  void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) override;
  bool IdleTasksEnabled(Isolate* isolate) override;
  double MonotonicallyIncreasingTime() override;
  v8::TracingController* GetTracingController() override;
  StackTracePrinter GetStackTracePrinter() override;

 private:
  static const int kMaxThreadPoolSize;

  Task* PopTaskInMainThreadQueue(v8::Isolate* isolate);
  Task* PopTaskInMainThreadDelayedQueue(v8::Isolate* isolate);
  IdleTask* PopTaskInMainThreadIdleQueue(v8::Isolate* isolate);

  void WaitForForegroundWork(v8::Isolate* isolate);
  void ScheduleOnForegroundThread(v8::Isolate* isolate, Task* task);

  base::Mutex lock_;
  bool initialized_;
  int thread_pool_size_;
  IdleTaskSupport idle_task_support_;
  std::vector<WorkerThread*> thread_pool_;
  TaskQueue queue_;
  std::map<v8::Isolate*, std::queue<Task*>> main_thread_queue_;
  std::map<v8::Isolate*, std::queue<IdleTask*>> main_thread_idle_queue_;
  std::map<v8::Isolate*, std::unique_ptr<base::Semaphore>> event_loop_control_;

  typedef std::pair<double, Task*> DelayedEntry;
  std::map<v8::Isolate*,
           std::priority_queue<DelayedEntry, std::vector<DelayedEntry>,
                               std::greater<DelayedEntry> > >
      main_thread_delayed_queue_;
  std::unique_ptr<TracingController> tracing_controller_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPlatform);
};


}  // namespace platform
}  // namespace v8


#endif  // V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
