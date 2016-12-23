// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
#define V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_

#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/inspector/string-16.h"
#include "src/locked-queue-inl.h"

class TaskRunner : public v8::base::Thread {
 public:
  class Task {
   public:
    virtual ~Task() {}
    virtual bool is_inspector_task() = 0;
    virtual void Run(v8::Isolate* isolate,
                     const v8::Global<v8::Context>& context) = 0;
  };

  TaskRunner(v8::ExtensionConfiguration* extensions, bool catch_exceptions,
             v8::base::Semaphore* ready_semaphore);
  virtual ~TaskRunner();

  // Thread implementation.
  void Run() override;

  // Should be called from the same thread and only from task.
  void RunMessageLoop(bool only_protocol);
  void QuitMessageLoop();

  // TaskRunner takes ownership.
  void Append(Task* task);

  static TaskRunner* FromContext(v8::Local<v8::Context>);

 private:
  void InitializeContext();
  Task* GetNext(bool only_protocol);

  v8::ExtensionConfiguration* extensions_;
  bool catch_exceptions_;
  v8::base::Semaphore* ready_semaphore_;

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;

  // deferred_queue_ combined with queue_ (in this order) have all tasks in the
  // correct order.
  // Sometimes we skip non-protocol tasks by moving them from queue_ to
  // deferred_queue_.
  v8::internal::LockedQueue<Task*> queue_;
  v8::internal::LockedQueue<Task*> deffered_queue_;
  v8::base::Semaphore process_queue_semaphore_;

  int nested_loop_count_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

class ExecuteStringTask : public TaskRunner::Task {
 public:
  explicit ExecuteStringTask(const v8_inspector::String16& expression);
  bool is_inspector_task() override { return false; }

  void Run(v8::Isolate* isolate,
           const v8::Global<v8::Context>& context) override;

 private:
  v8_inspector::String16 expression_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteStringTask);
};

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
