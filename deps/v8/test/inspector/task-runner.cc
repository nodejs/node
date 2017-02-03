// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/task-runner.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif               // !defined(_WIN32) && !defined(_WIN64)

namespace {

const int kTaskRunnerIndex = 2;

void ReportUncaughtException(v8::Isolate* isolate,
                             const v8::TryCatch& try_catch) {
  CHECK(try_catch.HasCaught());
  v8::HandleScope handle_scope(isolate);
  std::string message = *v8::String::Utf8Value(try_catch.Message()->Get());
  fprintf(stderr, "Unhandle exception: %s\n", message.data());
}

}  //  namespace

TaskRunner::TaskRunner(v8::ExtensionConfiguration* extensions,
                       bool catch_exceptions,
                       v8::base::Semaphore* ready_semaphore)
    : Thread(Options("Task Runner")),
      extensions_(extensions),
      catch_exceptions_(catch_exceptions),
      ready_semaphore_(ready_semaphore),
      isolate_(nullptr),
      process_queue_semaphore_(0),
      nested_loop_count_(0) {
  Start();
}

TaskRunner::~TaskRunner() { Join(); }

void TaskRunner::InitializeContext() {
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate_ = v8::Isolate::New(params);
  isolate_->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate_);
  v8::Local<v8::Context> context =
      v8::Context::New(isolate_, extensions_, global_template);
  context->SetAlignedPointerInEmbedderData(kTaskRunnerIndex, this);
  context_.Reset(isolate_, context);

  if (ready_semaphore_) ready_semaphore_->Signal();
}

void TaskRunner::Run() {
  InitializeContext();
  RunMessageLoop(false);
}

void TaskRunner::RunMessageLoop(bool only_protocol) {
  int loop_number = ++nested_loop_count_;
  while (nested_loop_count_ == loop_number) {
    TaskRunner::Task* task = GetNext(only_protocol);
    v8::Isolate::Scope isolate_scope(isolate_);
    if (catch_exceptions_) {
      v8::TryCatch try_catch(isolate_);
      task->Run(isolate_, context_);
      delete task;
      if (try_catch.HasCaught()) {
        ReportUncaughtException(isolate_, try_catch);
        fflush(stdout);
        fflush(stderr);
        _exit(0);
      }
    } else {
      task->Run(isolate_, context_);
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

TaskRunner::Task* TaskRunner::GetNext(bool only_protocol) {
  for (;;) {
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
  UNREACHABLE();
  return nullptr;
}

TaskRunner* TaskRunner::FromContext(v8::Local<v8::Context> context) {
  return static_cast<TaskRunner*>(
      context->GetAlignedPointerFromEmbedderData(kTaskRunnerIndex));
}

ExecuteStringTask::ExecuteStringTask(const v8_inspector::String16& expression)
    : expression_(expression) {}

void ExecuteStringTask::Run(v8::Isolate* isolate,
                            const v8::Global<v8::Context>& context) {
  v8::MicrotasksScope microtasks_scope(isolate,
                                       v8::MicrotasksScope::kRunMicrotasks);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> local_context = context.Get(isolate);
  v8::Context::Scope context_scope(local_context);

  v8::ScriptOrigin origin(v8::String::Empty(isolate));
  v8::Local<v8::String> source =
      v8::String::NewFromTwoByte(isolate, expression_.characters16(),
                                 v8::NewStringType::kNormal,
                                 static_cast<int>(expression_.length()))
          .ToLocalChecked();

  v8::ScriptCompiler::Source scriptSource(source, origin);
  v8::Local<v8::Script> script;
  if (!v8::ScriptCompiler::Compile(local_context, &scriptSource)
           .ToLocal(&script))
    return;
  v8::MaybeLocal<v8::Value> result;
  result = script->Run(local_context);
}
