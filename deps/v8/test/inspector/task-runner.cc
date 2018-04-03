// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/task-runner.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif               // !defined(_WIN32) && !defined(_WIN64)

namespace {

void ReportUncaughtException(v8::Isolate* isolate,
                             const v8::TryCatch& try_catch) {
  CHECK(try_catch.HasCaught());
  v8::HandleScope handle_scope(isolate);
  std::string message =
      *v8::String::Utf8Value(isolate, try_catch.Message()->Get());
  int line = try_catch.Message()
                 ->GetLineNumber(isolate->GetCurrentContext())
                 .FromJust();
  std::string source_line = *v8::String::Utf8Value(
      isolate, try_catch.Message()
                   ->GetSourceLine(isolate->GetCurrentContext())
                   .ToLocalChecked());
  fprintf(stderr, "Unhandle exception: %s @%s[%d]\n", message.data(),
          source_line.data(), line);
}

}  //  namespace

TaskRunner::TaskRunner(IsolateData::SetupGlobalTasks setup_global_tasks,
                       bool catch_exceptions,
                       v8::base::Semaphore* ready_semaphore,
                       v8::StartupData* startup_data, bool with_inspector)
    : Thread(Options("Task Runner")),
      setup_global_tasks_(std::move(setup_global_tasks)),
      startup_data_(startup_data),
      with_inspector_(with_inspector),
      catch_exceptions_(catch_exceptions),
      ready_semaphore_(ready_semaphore),
      data_(nullptr),
      process_queue_semaphore_(0),
      nested_loop_count_(0) {
  Start();
}

TaskRunner::~TaskRunner() { Join(); }

void TaskRunner::Run() {
  data_.reset(new IsolateData(this, std::move(setup_global_tasks_),
                              startup_data_, with_inspector_));
  if (ready_semaphore_) ready_semaphore_->Signal();
  RunMessageLoop(false);
}

void TaskRunner::RunMessageLoop(bool only_protocol) {
  int loop_number = ++nested_loop_count_;
  while (nested_loop_count_ == loop_number && !is_terminated_.Value()) {
    TaskRunner::Task* task = GetNext(only_protocol);
    if (!task) return;
    v8::Isolate::Scope isolate_scope(isolate());
    if (catch_exceptions_) {
      v8::TryCatch try_catch(isolate());
      task->Run(data_.get());
      delete task;
      if (try_catch.HasCaught()) {
        ReportUncaughtException(isolate(), try_catch);
        fflush(stdout);
        fflush(stderr);
        _exit(0);
      }
    } else {
      task->Run(data_.get());
      delete task;
    }
  }
}

void TaskRunner::QuitMessageLoop() {
  DCHECK_LT(0, nested_loop_count_);
  --nested_loop_count_;
}

void TaskRunner::Append(Task* task) {
  queue_.Enqueue(task);
  process_queue_semaphore_.Signal();
}

void TaskRunner::Terminate() {
  is_terminated_.Increment(1);
  process_queue_semaphore_.Signal();
}

TaskRunner::Task* TaskRunner::GetNext(bool only_protocol) {
  for (;;) {
    if (is_terminated_.Value()) return nullptr;
    if (only_protocol) {
      Task* task = nullptr;
      if (queue_.Dequeue(&task)) {
        if (task->is_priority_task()) return task;
        deffered_queue_.Enqueue(task);
      }
    } else {
      Task* task = nullptr;
      if (deffered_queue_.Dequeue(&task)) return task;
      if (queue_.Dequeue(&task)) return task;
    }
    process_queue_semaphore_.Wait();
  }
  return nullptr;
}
