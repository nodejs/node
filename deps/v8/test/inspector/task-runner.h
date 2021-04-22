// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
#define V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_

#include <map>
#include <memory>

#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/utils/locked-queue-inl.h"
#include "src/utils/vector.h"
#include "test/inspector/isolate-data.h"

namespace v8 {
namespace internal {

enum CatchExceptions {
  kFailOnUncaughtExceptions,
  kStandardPropagateUncaughtExceptions,
  kSuppressUncaughtExceptions
};

class TaskRunner : public v8::base::Thread {
 public:
  class Task {
   public:
    virtual ~Task() = default;
    virtual bool is_priority_task() = 0;
    virtual void Run(IsolateData* data) = 0;
  };

  TaskRunner(IsolateData::SetupGlobalTasks setup_global_tasks,
             CatchExceptions catch_exceptions,
             v8::base::Semaphore* ready_semaphore,
             v8::StartupData* startup_data, WithInspector with_inspector);
  ~TaskRunner() override;
  TaskRunner(const TaskRunner&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;
  IsolateData* data() const { return data_.get(); }

  // Thread implementation.
  void Run() override;

  // Should be called from the same thread and only from task.
  void RunMessageLoop(bool only_protocol);
  void QuitMessageLoop();

  void Append(std::unique_ptr<Task>);
  void InterruptForMessages();
  void Terminate();

 private:
  std::unique_ptr<Task> GetNext(bool only_protocol);
  v8::Isolate* isolate() const { return data_->isolate(); }

  IsolateData::SetupGlobalTasks setup_global_tasks_;
  v8::StartupData* startup_data_;
  WithInspector with_inspector_;
  CatchExceptions catch_exceptions_;
  v8::base::Semaphore* ready_semaphore_;
  std::unique_ptr<IsolateData> data_;

  // deferred_queue_ combined with queue_ (in this order) have all tasks in the
  // correct order. Sometimes we skip non-protocol tasks by moving them from
  // queue_ to deferred_queue_.
  v8::internal::LockedQueue<std::unique_ptr<Task>> queue_;
  v8::internal::LockedQueue<std::unique_ptr<Task>> deferred_queue_;
  v8::base::Semaphore process_queue_semaphore_;

  int nested_loop_count_;
  std::atomic<int> is_terminated_;
};

}  // namespace internal
}  // namespace v8

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
