// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
#define V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_

#include <map>

#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/locked-queue-inl.h"
#include "src/vector.h"
#include "test/inspector/isolate-data.h"

class TaskRunner : public v8::base::Thread {
 public:
  class Task {
   public:
    virtual ~Task() {}
    virtual bool is_inspector_task() = 0;
    void RunOnIsolate(IsolateData* data) {
      data_ = data;
      Run();
      data_ = nullptr;
    }

   protected:
    virtual void Run() = 0;
    v8::Isolate* isolate() const { return data_->isolate(); }
    IsolateData* data() const { return data_; }

   private:
    IsolateData* data_ = nullptr;
  };

  TaskRunner(IsolateData::SetupGlobalTasks setup_global_tasks,
             bool catch_exceptions, v8::base::Semaphore* ready_semaphore,
             v8::StartupData* startup_data,
             InspectorClientImpl::FrontendChannel* channel);
  virtual ~TaskRunner();
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
  InspectorClientImpl::FrontendChannel* channel_;
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

  v8::base::AtomicNumber<int> is_terminated_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

class AsyncTask : public TaskRunner::Task {
 public:
  AsyncTask(IsolateData* data, const char* task_name);
  virtual ~AsyncTask() = default;

 protected:
  virtual void AsyncRun() = 0;
  void Run() override;

  bool instrumenting_;
};

class ExecuteStringTask : public AsyncTask {
 public:
  ExecuteStringTask(IsolateData* data, int context_group_id,
                    const char* task_name,
                    const v8::internal::Vector<uint16_t>& expression,
                    v8::Local<v8::String> name,
                    v8::Local<v8::Integer> line_offset,
                    v8::Local<v8::Integer> column_offset,
                    v8::Local<v8::Boolean> is_module);
  ExecuteStringTask(const v8::internal::Vector<const char>& expression,
                    int context_group_id);
  bool is_inspector_task() override { return false; }

 private:
  void AsyncRun() override;

  v8::internal::Vector<uint16_t> expression_;
  v8::internal::Vector<const char> expression_utf8_;
  v8::internal::Vector<uint16_t> name_;
  int32_t line_offset_ = 0;
  int32_t column_offset_ = 0;
  bool is_module_ = false;
  int context_group_id_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteStringTask);
};

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_TASK_RUNNER_H_
