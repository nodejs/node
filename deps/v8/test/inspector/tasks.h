// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_TASKS_H_
#define V8_TEST_INSPECTOR_TASKS_H_

#include <vector>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/base/platform/semaphore.h"
#include "test/inspector/isolate-data.h"
#include "test/inspector/task-runner.h"
#include "test/inspector/utils.h"

namespace v8 {
namespace internal {

template <typename T>
void RunSyncTask(TaskRunner* task_runner, T callback) {
  class SyncTask : public TaskRunner::Task {
   public:
    SyncTask(v8::base::Semaphore* ready_semaphore, T callback)
        : ready_semaphore_(ready_semaphore), callback_(callback) {}
    ~SyncTask() override = default;
    bool is_priority_task() final { return true; }

   private:
    void Run(IsolateData* data) override {
      callback_(data);
      if (ready_semaphore_) ready_semaphore_->Signal();
    }

    v8::base::Semaphore* ready_semaphore_;
    T callback_;
  };

  v8::base::Semaphore ready_semaphore(0);
  task_runner->Append(std::make_unique<SyncTask>(&ready_semaphore, callback));
  ready_semaphore.Wait();
}

class SendMessageToBackendTask : public TaskRunner::Task {
 public:
  SendMessageToBackendTask(int session_id, const std::vector<uint16_t>& message)
      : session_id_(session_id), message_(message) {}
  bool is_priority_task() final { return true; }

 private:
  void Run(IsolateData* data) override {
    v8_inspector::StringView message_view(message_.data(), message_.size());
    data->SendMessage(session_id_, message_view);
  }

  int session_id_;
  std::vector<uint16_t> message_;
};

inline void RunAsyncTask(TaskRunner* task_runner,
                         const v8_inspector::StringView& task_name,
                         std::unique_ptr<TaskRunner::Task> task) {
  class AsyncTask : public TaskRunner::Task {
   public:
    explicit AsyncTask(std::unique_ptr<TaskRunner::Task> inner)
        : inner_(std::move(inner)) {}
    ~AsyncTask() override = default;
    AsyncTask(const AsyncTask&) = delete;
    AsyncTask& operator=(const AsyncTask&) = delete;
    bool is_priority_task() override { return inner_->is_priority_task(); }
    void Run(IsolateData* data) override {
      data->AsyncTaskStarted(inner_.get());
      inner_->Run(data);
      data->AsyncTaskFinished(inner_.get());
    }

   private:
    std::unique_ptr<TaskRunner::Task> inner_;
  };

  task_runner->data()->AsyncTaskScheduled(task_name, task.get(), false);
  task_runner->Append(std::make_unique<AsyncTask>(std::move(task)));
}

class ExecuteStringTask : public TaskRunner::Task {
 public:
  ExecuteStringTask(v8::Isolate* isolate, int context_group_id,
                    const std::vector<uint16_t>& expression,
                    v8::Local<v8::String> name,
                    v8::Local<v8::Integer> line_offset,
                    v8::Local<v8::Integer> column_offset,
                    v8::Local<v8::Boolean> is_module)
      : expression_(expression),
        name_(ToVector(isolate, name)),
        line_offset_(line_offset.As<v8::Int32>()->Value()),
        column_offset_(column_offset.As<v8::Int32>()->Value()),
        is_module_(is_module->Value()),
        context_group_id_(context_group_id) {}

  ExecuteStringTask(const std::string& expression, int context_group_id)
      : expression_utf8_(expression), context_group_id_(context_group_id) {}

  ~ExecuteStringTask() override = default;
  ExecuteStringTask(const ExecuteStringTask&) = delete;
  ExecuteStringTask& operator=(const ExecuteStringTask&) = delete;
  bool is_priority_task() override { return false; }
  void Run(IsolateData* data) override;

 private:
  std::vector<uint16_t> expression_;
  std::string expression_utf8_;
  std::vector<uint16_t> name_;
  int32_t line_offset_ = 0;
  int32_t column_offset_ = 0;
  bool is_module_ = false;
  int context_group_id_;
};

class SetTimeoutTask : public TaskRunner::Task {
 public:
  SetTimeoutTask(int context_group_id, v8::Isolate* isolate,
                 v8::Local<v8::Function> function)
      : function_(isolate, function), context_group_id_(context_group_id) {}
  ~SetTimeoutTask() override = default;
  bool is_priority_task() final { return false; }

 private:
  void Run(IsolateData* data) override {
    v8::MicrotasksScope microtasks_scope(data->isolate(),
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::HandleScope handle_scope(data->isolate());
    v8::Local<v8::Context> context = data->GetDefaultContext(context_group_id_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Function> function = function_.Get(data->isolate());
    v8::MaybeLocal<v8::Value> result;
    result = function->Call(context, context->Global(), 0, nullptr);
  }

  v8::Global<v8::Function> function_;
  int context_group_id_;
};

class SetTimeoutExtension : public IsolateData::SetupGlobalTask {
 public:
  void Run(v8::Isolate* isolate,
           v8::Local<v8::ObjectTemplate> global) override {
    global->Set(
        ToV8String(isolate, "setTimeout"),
        v8::FunctionTemplate::New(isolate, &SetTimeoutExtension::SetTimeout));
  }

 private:
  static void SetTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[1]->IsNumber() ||
        (!args[0]->IsFunction() && !args[0]->IsString()) ||
        args[1].As<v8::Number>()->Value() != 0.0) {
      return;
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    int context_group_id = data->GetContextGroupId(context);
    const char* task_name = "setTimeout";
    v8_inspector::StringView task_name_view(
        reinterpret_cast<const uint8_t*>(task_name), strlen(task_name));
    if (args[0]->IsFunction()) {
      RunAsyncTask(data->task_runner(), task_name_view,
                   std::make_unique<SetTimeoutTask>(
                       context_group_id, isolate,
                       v8::Local<v8::Function>::Cast(args[0])));
    } else {
      RunAsyncTask(
          data->task_runner(), task_name_view,
          std::make_unique<ExecuteStringTask>(
              isolate, context_group_id,
              ToVector(isolate, args[0].As<v8::String>()),
              v8::String::Empty(isolate), v8::Integer::New(isolate, 0),
              v8::Integer::New(isolate, 0), v8::Boolean::New(isolate, false)));
    }
  }
};

}  // namespace internal
}  // namespace v8

#endif  //  V8_TEST_INSPECTOR_TASKS_H_
