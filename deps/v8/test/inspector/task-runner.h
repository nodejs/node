// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
#define V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_

#include <map>

#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/utils/locked-queue-inl.h"
#include "src/utils/vector.h"
#include "test/inspector/isolate-data.h"

class TaskRunner : public v8::base::Thread {
 public:
  class Task {
   public:
    virtual ~Task() = default;
    virtual bool is_priority_task() = 0;
    virtual void Run(IsolateData* data) = 0;
  };

  TaskRunner(IsolateData::SetupGlobalTasks setup_global_tasks,
             bool catch_exceptions, v8::base::Semaphore* ready_semaphore,
             v8::StartupData* startup_data, bool with_inspector);
  ~TaskRunner() override;
  IsolateData* data() const { return data_.get(); }

  // Thread implementation.
  void Run() override;

  // Should be called from the same thread and only from task.
  void RunMessageLoop(bool only_protocol);
  void QuitMessageLoop();

  // TaskRunner takes ownership.
  void Append(Task* task);

  void Terminate();

 private:
  Task* GetNext(bool only_protocol);
  v8::Isolate* isolate() const { return data_->isolate(); }

  IsolateData::SetupGlobalTasks setup_global_tasks_;
  v8::StartupData* startup_data_;
  bool with_inspector_;
  bool catch_exceptions_;
  v8::base::Semaphore* ready_semaphore_;
  std::unique_ptr<IsolateData> data_;

  // deferred_queue_ combined with queue_ (in this order) have all tasks in the
  // correct order. Sometimes we skip non-protocol tasks by moving them from
  // queue_ to deferred_queue_.
  v8::internal::LockedQueue<Task*> queue_;
  v8::internal::LockedQueue<Task*> deffered_queue_;
  v8::base::Semaphore process_queue_semaphore_;

  int nested_loop_count_;

  std::atomic<int> is_terminated_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
