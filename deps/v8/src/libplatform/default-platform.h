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

#include "include/libplatform/v8-tracing.h"
#include "include/v8-platform.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/libplatform/task-queue.h"

namespace v8 {
namespace platform {

class TaskQueue;
class Thread;
class WorkerThread;

namespace tracing {
class TracingController;
}

class DefaultPlatform : public Platform {
 public:
  DefaultPlatform();
  virtual ~DefaultPlatform();

  void SetThreadPoolSize(int thread_pool_size);

  void EnsureInitialized();

  bool PumpMessageLoop(v8::Isolate* isolate);

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
  const uint8_t* GetCategoryGroupEnabled(const char* name) override;
  const char* GetCategoryGroupName(
      const uint8_t* category_enabled_flag) override;
  using Platform::AddTraceEvent;
  uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) override;
  void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                const char* name, uint64_t handle) override;
  void SetTracingController(tracing::TracingController* tracing_controller);

  void AddTraceStateObserver(TraceStateObserver* observer) override;
  void RemoveTraceStateObserver(TraceStateObserver* observer) override;

 private:
  static const int kMaxThreadPoolSize;

  Task* PopTaskInMainThreadQueue(v8::Isolate* isolate);
  Task* PopTaskInMainThreadDelayedQueue(v8::Isolate* isolate);

  base::Mutex lock_;
  bool initialized_;
  int thread_pool_size_;
  std::vector<WorkerThread*> thread_pool_;
  TaskQueue queue_;
  std::map<v8::Isolate*, std::queue<Task*> > main_thread_queue_;

  typedef std::pair<double, Task*> DelayedEntry;
  std::map<v8::Isolate*,
           std::priority_queue<DelayedEntry, std::vector<DelayedEntry>,
                               std::greater<DelayedEntry> > >
      main_thread_delayed_queue_;
  std::unique_ptr<tracing::TracingController> tracing_controller_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPlatform);
};


}  // namespace platform
}  // namespace v8


#endif  // V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
