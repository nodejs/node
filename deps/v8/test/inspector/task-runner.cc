// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/task-runner.h"

#include "test/inspector/inspector-impl.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif               // !defined(_WIN32) && !defined(_WIN64)

namespace {

void ReportUncaughtException(v8::Isolate* isolate,
                             const v8::TryCatch& try_catch) {
  CHECK(try_catch.HasCaught());
  v8::HandleScope handle_scope(isolate);
  std::string message = *v8::String::Utf8Value(try_catch.Message()->Get());
  int line = try_catch.Message()
                 ->GetLineNumber(isolate->GetCurrentContext())
                 .FromJust();
  std::string source_line =
      *v8::String::Utf8Value(try_catch.Message()
                                 ->GetSourceLine(isolate->GetCurrentContext())
                                 .ToLocalChecked());
  fprintf(stderr, "Unhandle exception: %s @%s[%d]\n", message.data(),
          source_line.data(), line);
}

v8::internal::Vector<uint16_t> ToVector(v8::Local<v8::String> str) {
  v8::internal::Vector<uint16_t> buffer =
      v8::internal::Vector<uint16_t>::New(str->Length());
  str->Write(buffer.start(), 0, str->Length());
  return buffer;
}

}  //  namespace

TaskRunner::TaskRunner(IsolateData::SetupGlobalTasks setup_global_tasks,
                       bool catch_exceptions,
                       v8::base::Semaphore* ready_semaphore,
                       v8::StartupData* startup_data,
                       InspectorClientImpl::FrontendChannel* channel)
    : Thread(Options("Task Runner")),
      setup_global_tasks_(std::move(setup_global_tasks)),
      startup_data_(startup_data),
      channel_(channel),
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
                              startup_data_, channel_));
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
      task->RunOnIsolate(data_.get());
      delete task;
      if (try_catch.HasCaught()) {
        ReportUncaughtException(isolate(), try_catch);
        fflush(stdout);
        fflush(stderr);
        _exit(0);
      }
    } else {
      task->RunOnIsolate(data_.get());
      delete task;
    }
  }
}

void TaskRunner::QuitMessageLoop() {
  DCHECK(nested_loop_count_ > 0);
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
        if (task->is_inspector_task()) return task;
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

AsyncTask::AsyncTask(IsolateData* data, const char* task_name)
    : instrumenting_(data && task_name) {
  if (!instrumenting_) return;
  data->inspector()->inspector()->asyncTaskScheduled(
      v8_inspector::StringView(reinterpret_cast<const uint8_t*>(task_name),
                               strlen(task_name)),
      this, false);
}

void AsyncTask::Run() {
  if (instrumenting_) data()->inspector()->inspector()->asyncTaskStarted(this);
  AsyncRun();
  if (instrumenting_) data()->inspector()->inspector()->asyncTaskFinished(this);
}

ExecuteStringTask::ExecuteStringTask(
    IsolateData* data, int context_group_id, const char* task_name,
    const v8::internal::Vector<uint16_t>& expression,
    v8::Local<v8::String> name, v8::Local<v8::Integer> line_offset,
    v8::Local<v8::Integer> column_offset, v8::Local<v8::Boolean> is_module)
    : AsyncTask(data, task_name),
      expression_(expression),
      name_(ToVector(name)),
      line_offset_(line_offset.As<v8::Int32>()->Value()),
      column_offset_(column_offset.As<v8::Int32>()->Value()),
      is_module_(is_module->Value()),
      context_group_id_(context_group_id) {}

ExecuteStringTask::ExecuteStringTask(
    const v8::internal::Vector<const char>& expression, int context_group_id)
    : AsyncTask(nullptr, nullptr),
      expression_utf8_(expression),
      context_group_id_(context_group_id) {}

void ExecuteStringTask::AsyncRun() {
  v8::MicrotasksScope microtasks_scope(isolate(),
                                       v8::MicrotasksScope::kRunMicrotasks);
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = data()->GetContext(context_group_id_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> name =
      v8::String::NewFromTwoByte(isolate(), name_.start(),
                                 v8::NewStringType::kNormal, name_.length())
          .ToLocalChecked();
  v8::Local<v8::Integer> line_offset =
      v8::Integer::New(isolate(), line_offset_);
  v8::Local<v8::Integer> column_offset =
      v8::Integer::New(isolate(), column_offset_);

  v8::ScriptOrigin origin(
      name, line_offset, column_offset,
      /* resource_is_shared_cross_origin */ v8::Local<v8::Boolean>(),
      /* script_id */ v8::Local<v8::Integer>(),
      /* source_map_url */ v8::Local<v8::Value>(),
      /* resource_is_opaque */ v8::Local<v8::Boolean>(),
      /* is_wasm */ v8::Local<v8::Boolean>(),
      v8::Boolean::New(isolate(), is_module_));
  v8::Local<v8::String> source;
  if (expression_.length()) {
    source = v8::String::NewFromTwoByte(isolate(), expression_.start(),
                                        v8::NewStringType::kNormal,
                                        expression_.length())
                 .ToLocalChecked();
  } else {
    source = v8::String::NewFromUtf8(isolate(), expression_utf8_.start(),
                                     v8::NewStringType::kNormal,
                                     expression_utf8_.length())
                 .ToLocalChecked();
  }

  v8::ScriptCompiler::Source scriptSource(source, origin);
  if (!is_module_) {
    v8::Local<v8::Script> script;
    if (!v8::ScriptCompiler::Compile(context, &scriptSource).ToLocal(&script))
      return;
    v8::MaybeLocal<v8::Value> result;
    result = script->Run(context);
  } else {
    data()->RegisterModule(context, name_, &scriptSource);
  }
}
