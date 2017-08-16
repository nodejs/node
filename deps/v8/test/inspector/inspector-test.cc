// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif               // !defined(_WIN32) && !defined(_WIN64)

#include <locale.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include "src/base/platform/platform.h"
#include "src/flags.h"
#include "src/inspector/test-interface.h"
#include "src/utils.h"
#include "src/vector.h"

#include "test/inspector/inspector-impl.h"
#include "test/inspector/task-runner.h"

namespace {

std::vector<TaskRunner*> task_runners;

void Terminate() {
  for (size_t i = 0; i < task_runners.size(); ++i) {
    task_runners[i]->Terminate();
    task_runners[i]->Join();
  }
  std::vector<TaskRunner*> empty;
  task_runners.swap(empty);
}

void Exit() {
  fflush(stdout);
  fflush(stderr);
  Terminate();
}

v8::internal::Vector<uint16_t> ToVector(v8::Local<v8::String> str) {
  v8::internal::Vector<uint16_t> buffer =
      v8::internal::Vector<uint16_t>::New(str->Length());
  str->Write(buffer.start(), 0, str->Length());
  return buffer;
}

v8::Local<v8::String> ToV8String(v8::Isolate* isolate, const char* str) {
  return v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kNormal)
      .ToLocalChecked();
}

v8::internal::Vector<uint16_t> ToVector(
    const v8_inspector::StringView& string) {
  v8::internal::Vector<uint16_t> buffer =
      v8::internal::Vector<uint16_t>::New(static_cast<int>(string.length()));
  for (size_t i = 0; i < string.length(); i++) {
    if (string.is8Bit())
      buffer[i] = string.characters8()[i];
    else
      buffer[i] = string.characters16()[i];
  }
  return buffer;
}

class CreateContextGroupTask : public TaskRunner::Task {
 public:
  CreateContextGroupTask(v8::base::Semaphore* ready_semaphore,
                         int* context_group_id)
      : ready_semaphore_(ready_semaphore),
        context_group_id_(context_group_id) {}
  virtual ~CreateContextGroupTask() = default;
  bool is_inspector_task() final { return true; }

 private:
  void Run() override {
    *context_group_id_ = data()->CreateContextGroup();
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

  v8::base::Semaphore* ready_semaphore_;
  int* context_group_id_;
};

class ConnectSessionTask : public TaskRunner::Task {
 public:
  ConnectSessionTask(v8::base::Semaphore* ready_semaphore, int context_group_id,
                     const v8::internal::Vector<uint16_t>& state,
                     int* session_id)
      : ready_semaphore_(ready_semaphore),
        context_group_id_(context_group_id),
        state_(state),
        session_id_(session_id) {}
  virtual ~ConnectSessionTask() = default;
  bool is_inspector_task() final { return true; }

 private:
  void Run() override {
    v8_inspector::StringView state(state_.start(), state_.length());
    *session_id_ =
        data()->inspector()->ConnectSession(context_group_id_, state);
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

  v8::base::Semaphore* ready_semaphore_;
  int context_group_id_;
  const v8::internal::Vector<uint16_t>& state_;
  int* session_id_;
};

class DisconnectSessionTask : public TaskRunner::Task {
 public:
  DisconnectSessionTask(v8::base::Semaphore* ready_semaphore, int session_id,
                        v8::internal::Vector<uint16_t>* state)
      : ready_semaphore_(ready_semaphore),
        session_id_(session_id),
        state_(state) {}
  virtual ~DisconnectSessionTask() = default;
  bool is_inspector_task() final { return true; }

 private:
  void Run() override {
    std::unique_ptr<v8_inspector::StringBuffer> state =
        data()->inspector()->DisconnectSession(session_id_);
    *state_ = ToVector(state->string());
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

  v8::base::Semaphore* ready_semaphore_;
  int session_id_;
  v8::internal::Vector<uint16_t>* state_;
};

class SendMessageToBackendTask : public TaskRunner::Task {
 public:
  explicit SendMessageToBackendTask(
      int session_id, const v8::internal::Vector<uint16_t>& message)
      : session_id_(session_id), message_(message) {}
  bool is_inspector_task() final { return true; }

 private:
  void Run() override {
    v8_inspector::StringView message_view(message_.start(), message_.length());
    data()->inspector()->SendMessage(session_id_, message_view);
  }

  int session_id_;
  v8::internal::Vector<uint16_t> message_;
};

class SchedulePauseOnNextStatementTask : public TaskRunner::Task {
 public:
  SchedulePauseOnNextStatementTask(
      v8::base::Semaphore* ready_semaphore, int context_group_id,
      const v8::internal::Vector<uint16_t>& reason,
      const v8::internal::Vector<uint16_t>& details)
      : ready_semaphore_(ready_semaphore),
        context_group_id_(context_group_id),
        reason_(reason),
        details_(details) {}
  virtual ~SchedulePauseOnNextStatementTask() = default;
  bool is_inspector_task() final { return true; }

 private:
  void Run() override {
    v8_inspector::StringView reason(reason_.start(), reason_.length());
    v8_inspector::StringView details(details_.start(), details_.length());
    data()->inspector()->SchedulePauseOnNextStatement(context_group_id_, reason,
                                                      details);
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

  v8::base::Semaphore* ready_semaphore_;
  int context_group_id_;
  const v8::internal::Vector<uint16_t>& reason_;
  const v8::internal::Vector<uint16_t>& details_;
};

class CancelPauseOnNextStatementTask : public TaskRunner::Task {
 public:
  CancelPauseOnNextStatementTask(v8::base::Semaphore* ready_semaphore,
                                 int context_group_id)
      : ready_semaphore_(ready_semaphore),
        context_group_id_(context_group_id) {}
  virtual ~CancelPauseOnNextStatementTask() = default;
  bool is_inspector_task() final { return true; }

 private:
  void Run() override {
    data()->inspector()->CancelPauseOnNextStatement(context_group_id_);
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

  v8::base::Semaphore* ready_semaphore_;
  int context_group_id_;
};

class SendMessageToFrontendTask : public TaskRunner::Task {
 public:
  SendMessageToFrontendTask(int context_group_id, int session_id,
                            const v8::internal::Vector<uint16_t>& message)
      : context_group_id_(context_group_id),
        session_id_(session_id),
        message_(message) {}
  virtual ~SendMessageToFrontendTask() {}

  bool is_inspector_task() final { return false; }

  static void Register(int session_id, v8::Isolate* isolate,
                       v8::Local<v8::Function> dispatcher) {
    dispatchers_[session_id].Reset(isolate, dispatcher);
  }

  static void Unregister(int session_id) { dispatchers_.erase(session_id); }

 private:
  void Run() override {
    v8::MicrotasksScope microtasks_scope(isolate(),
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = data()->GetContext(context_group_id_);
    v8::Context::Scope context_scope(context);

    if (dispatchers_.find(session_id_) == dispatchers_.end()) return;
    v8::Local<v8::Function> function = dispatchers_[session_id_].Get(isolate());
    v8::Local<v8::Value> message =
        v8::String::NewFromTwoByte(isolate(), message_.start(),
                                   v8::NewStringType::kNormal,
                                   static_cast<int>(message_.size()))
            .ToLocalChecked();
    v8::MaybeLocal<v8::Value> result;
    result = function->Call(context, context->Global(), 1, &message);
  }

  static std::map<int, v8::Global<v8::Function>> dispatchers_;
  int context_group_id_;
  int session_id_;
  v8::internal::Vector<uint16_t> message_;
};

std::map<int, v8::Global<v8::Function>> SendMessageToFrontendTask::dispatchers_;

class UtilsExtension : public IsolateData::SetupGlobalTask {
 public:
  ~UtilsExtension() override = default;
  void Run(v8::Isolate* isolate,
           v8::Local<v8::ObjectTemplate> global) override {
    v8::Local<v8::ObjectTemplate> utils = v8::ObjectTemplate::New(isolate);
    utils->Set(ToV8String(isolate, "print"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Print));
    utils->Set(ToV8String(isolate, "quit"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Quit));
    utils->Set(ToV8String(isolate, "setlocale"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Setlocale));
    utils->Set(ToV8String(isolate, "read"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Read));
    utils->Set(ToV8String(isolate, "load"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Load));
    utils->Set(ToV8String(isolate, "compileAndRunWithOrigin"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::CompileAndRunWithOrigin));
    utils->Set(ToV8String(isolate, "setCurrentTimeMSForTest"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetCurrentTimeMSForTest));
    utils->Set(ToV8String(isolate, "setMemoryInfoForTest"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetMemoryInfoForTest));
    utils->Set(ToV8String(isolate, "schedulePauseOnNextStatement"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SchedulePauseOnNextStatement));
    utils->Set(ToV8String(isolate, "cancelPauseOnNextStatement"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::CancelPauseOnNextStatement));
    utils->Set(ToV8String(isolate, "setLogConsoleApiMessageCalls"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetLogConsoleApiMessageCalls));
    utils->Set(ToV8String(isolate, "createContextGroup"),
               v8::FunctionTemplate::New(isolate,
                                         &UtilsExtension::CreateContextGroup));
    utils->Set(
        ToV8String(isolate, "connectSession"),
        v8::FunctionTemplate::New(isolate, &UtilsExtension::ConnectSession));
    utils->Set(
        ToV8String(isolate, "disconnectSession"),
        v8::FunctionTemplate::New(isolate, &UtilsExtension::DisconnectSession));
    utils->Set(ToV8String(isolate, "sendMessageToBackend"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SendMessageToBackend));
    global->Set(ToV8String(isolate, "utils"), utils);
  }

  static void set_backend_task_runner(TaskRunner* runner) {
    backend_runner_ = runner;
  }

 private:
  static TaskRunner* backend_runner_;

  static void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    for (int i = 0; i < args.Length(); i++) {
      v8::HandleScope handle_scope(args.GetIsolate());
      if (i != 0) {
        printf(" ");
      }

      // Explicitly catch potential exceptions in toString().
      v8::TryCatch try_catch(args.GetIsolate());
      v8::Local<v8::Value> arg = args[i];
      v8::Local<v8::String> str_obj;

      if (arg->IsSymbol()) {
        arg = v8::Local<v8::Symbol>::Cast(arg)->Name();
      }
      if (!arg->ToString(args.GetIsolate()->GetCurrentContext())
               .ToLocal(&str_obj)) {
        try_catch.ReThrow();
        return;
      }

      v8::String::Utf8Value str(str_obj);
      int n =
          static_cast<int>(fwrite(*str, sizeof(**str), str.length(), stdout));
      if (n != str.length()) {
        printf("Error in fwrite\n");
        Quit(args);
      }
    }
    printf("\n");
    fflush(stdout);
  }

  static void Quit(const v8::FunctionCallbackInfo<v8::Value>& args) { Exit(); }

  static void Setlocale(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      fprintf(stderr, "Internal error: setlocale get one string argument.");
      Exit();
    }
    v8::String::Utf8Value str(args[0]);
    setlocale(LC_NUMERIC, *str);
  }

  static bool ReadFile(v8::Isolate* isolate, v8::Local<v8::Value> name,
                       v8::internal::Vector<const char>* chars) {
    v8::String::Utf8Value str(name);
    bool exists = false;
    std::string filename(*str, str.length());
    *chars = v8::internal::ReadFile(filename.c_str(), &exists);
    if (!exists) {
      isolate->ThrowException(
          v8::String::NewFromUtf8(isolate, "Error reading file",
                                  v8::NewStringType::kNormal)
              .ToLocalChecked());
      return false;
    }
    return true;
  }

  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      fprintf(stderr, "Internal error: read gets one string argument.");
      Exit();
    }
    v8::internal::Vector<const char> chars;
    v8::Isolate* isolate = args.GetIsolate();
    if (ReadFile(isolate, args[0], &chars)) {
      args.GetReturnValue().Set(
          v8::String::NewFromUtf8(isolate, chars.start(),
                                  v8::NewStringType::kNormal, chars.length())
              .ToLocalChecked());
    }
  }

  static void Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      fprintf(stderr, "Internal error: load gets one string argument.");
      Exit();
    }
    v8::internal::Vector<const char> chars;
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    int context_group_id = data->GetContextGroupId(context);
    if (ReadFile(isolate, args[0], &chars)) {
      ExecuteStringTask(chars, context_group_id).RunOnIsolate(data);
    }
  }

  static void CompileAndRunWithOrigin(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 6 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsString() || !args[3]->IsInt32() || !args[4]->IsInt32() ||
        !args[5]->IsBoolean()) {
      fprintf(stderr,
              "Internal error: compileAndRunWithOrigin(context_group_id, "
              "source, name, line, "
              "column, is_module).");
      Exit();
    }

    backend_runner_->Append(new ExecuteStringTask(
        nullptr, args[0].As<v8::Int32>()->Value(), nullptr,
        ToVector(args[1].As<v8::String>()), args[2].As<v8::String>(),
        args[3].As<v8::Int32>(), args[4].As<v8::Int32>(),
        args[5].As<v8::Boolean>()));
  }

  static void SetCurrentTimeMSForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsNumber()) {
      fprintf(stderr, "Internal error: setCurrentTimeMSForTest(time).");
      Exit();
    }
    backend_runner_->data()->inspector()->SetCurrentTimeMSForTest(
        args[0].As<v8::Number>()->Value());
  }

  static void SetMemoryInfoForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1) {
      fprintf(stderr, "Internal error: setMemoryInfoForTest(value).");
      Exit();
    }
    backend_runner_->data()->inspector()->SetMemoryInfoForTest(args[0]);
  }

  static void SchedulePauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsString()) {
      fprintf(stderr,
              "Internal error: schedulePauseOnNextStatement(context_group_id, "
              "'reason', 'details').");
      Exit();
    }
    v8::internal::Vector<uint16_t> reason = ToVector(args[1].As<v8::String>());
    v8::internal::Vector<uint16_t> details = ToVector(args[2].As<v8::String>());
    v8::base::Semaphore ready_semaphore(0);
    backend_runner_->Append(new SchedulePauseOnNextStatementTask(
        &ready_semaphore, args[0].As<v8::Int32>()->Value(), reason, details));
    ready_semaphore.Wait();
  }

  static void CancelPauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr,
              "Internal error: cancelPauseOnNextStatement(context_group_id).");
      Exit();
    }
    v8::base::Semaphore ready_semaphore(0);
    backend_runner_->Append(new CancelPauseOnNextStatementTask(
        &ready_semaphore, args[0].As<v8::Int32>()->Value()));
    ready_semaphore.Wait();
  }

  static void SetLogConsoleApiMessageCalls(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      fprintf(stderr, "Internal error: setLogConsoleApiMessageCalls(bool).");
      Exit();
    }
    backend_runner_->data()->inspector()->SetLogConsoleApiMessageCalls(
        args[0].As<v8::Boolean>()->Value());
  }

  static void CreateContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: createContextGroup().");
      Exit();
    }
    v8::base::Semaphore ready_semaphore(0);
    int context_group_id = 0;
    backend_runner_->Append(
        new CreateContextGroupTask(&ready_semaphore, &context_group_id));
    ready_semaphore.Wait();
    args.GetReturnValue().Set(
        v8::Int32::New(args.GetIsolate(), context_group_id));
  }

  static void ConnectSession(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsFunction()) {
      fprintf(stderr,
              "Internal error: connectionSession(context_group_id, state, "
              "dispatch).");
      Exit();
    }
    v8::internal::Vector<uint16_t> state = ToVector(args[1].As<v8::String>());
    v8::base::Semaphore ready_semaphore(0);
    int session_id = 0;
    backend_runner_->Append(new ConnectSessionTask(
        &ready_semaphore, args[0].As<v8::Int32>()->Value(), state,
        &session_id));
    ready_semaphore.Wait();
    SendMessageToFrontendTask::Register(session_id, args.GetIsolate(),
                                        args[2].As<v8::Function>());
    args.GetReturnValue().Set(v8::Int32::New(args.GetIsolate(), session_id));
  }

  static void DisconnectSession(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr, "Internal error: disconnectionSession(session_id).");
      Exit();
    }
    int session_id = args[0].As<v8::Int32>()->Value();
    SendMessageToFrontendTask::Unregister(session_id);
    v8::base::Semaphore ready_semaphore(0);
    v8::internal::Vector<uint16_t> state;
    backend_runner_->Append(
        new DisconnectSessionTask(&ready_semaphore, session_id, &state));
    ready_semaphore.Wait();
    args.GetReturnValue().Set(
        v8::String::NewFromTwoByte(args.GetIsolate(), state.start(),
                                   v8::NewStringType::kNormal,
                                   static_cast<int>(state.size()))
            .ToLocalChecked());
  }

  static void SendMessageToBackend(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsInt32() || !args[1]->IsString()) {
      fprintf(stderr,
              "Internal error: sendMessageToBackend(session_id, message).");
      Exit();
    }
    backend_runner_->Append(new SendMessageToBackendTask(
        args[0].As<v8::Int32>()->Value(), ToVector(args[1].As<v8::String>())));
  }
};

TaskRunner* UtilsExtension::backend_runner_ = nullptr;

class SetTimeoutTask : public AsyncTask {
 public:
  SetTimeoutTask(IsolateData* data, int context_group_id, const char* task_name,
                 v8::Local<v8::Function> function)
      : AsyncTask(data, task_name),
        function_(data->isolate(), function),
        context_group_id_(context_group_id) {}
  virtual ~SetTimeoutTask() {}

  bool is_inspector_task() final { return false; }

 private:
  void AsyncRun() override {
    v8::MicrotasksScope microtasks_scope(isolate(),
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = data()->GetContext(context_group_id_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Function> function = function_.Get(isolate());
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
      fprintf(
          stderr,
          "Internal error: only setTimeout(function|code, 0) is supported.");
      Exit();
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    int context_group_id = data->GetContextGroupId(context);
    std::unique_ptr<TaskRunner::Task> task;
    if (args[0]->IsFunction()) {
      task.reset(new SetTimeoutTask(data, context_group_id, "setTimeout",
                                    v8::Local<v8::Function>::Cast(args[0])));
    } else {
      task.reset(new ExecuteStringTask(
          data, context_group_id, "setTimeout",
          ToVector(args[0].As<v8::String>()), v8::String::Empty(isolate),
          v8::Integer::New(isolate, 0), v8::Integer::New(isolate, 0),
          v8::Boolean::New(isolate, false)));
    }
    data->task_runner()->Append(task.release());
  }
};

bool StrictAccessCheck(v8::Local<v8::Context> accessing_context,
                       v8::Local<v8::Object> accessed_object,
                       v8::Local<v8::Value> data) {
  CHECK(accessing_context.IsEmpty());
  return accessing_context.IsEmpty();
}

class InspectorExtension : public IsolateData::SetupGlobalTask {
 public:
  ~InspectorExtension() override = default;
  void Run(v8::Isolate* isolate,
           v8::Local<v8::ObjectTemplate> global) override {
    v8::Local<v8::ObjectTemplate> inspector = v8::ObjectTemplate::New(isolate);
    inspector->Set(ToV8String(isolate, "fireContextCreated"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::FireContextCreated));
    inspector->Set(ToV8String(isolate, "fireContextDestroyed"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::FireContextDestroyed));
    inspector->Set(
        ToV8String(isolate, "freeContext"),
        v8::FunctionTemplate::New(isolate, &InspectorExtension::FreeContext));
    inspector->Set(ToV8String(isolate, "setMaxAsyncTaskStacks"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::SetMaxAsyncTaskStacks));
    inspector->Set(
        ToV8String(isolate, "dumpAsyncTaskStacksStateForTest"),
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::DumpAsyncTaskStacksStateForTest));
    inspector->Set(
        ToV8String(isolate, "breakProgram"),
        v8::FunctionTemplate::New(isolate, &InspectorExtension::BreakProgram));
    inspector->Set(
        ToV8String(isolate, "createObjectWithStrictCheck"),
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::CreateObjectWithStrictCheck));
    inspector->Set(ToV8String(isolate, "callWithScheduledBreak"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::CallWithScheduledBreak));
    inspector->Set(ToV8String(isolate, "allowAccessorFormatting"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::AllowAccessorFormatting));
    global->Set(ToV8String(isolate, "inspector"), inspector);
  }

 private:
  static void FireContextCreated(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->inspector()->ContextCreated(context,
                                      data->GetContextGroupId(context));
  }

  static void FireContextDestroyed(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->inspector()->ContextDestroyed(context);
  }

  static void FreeContext(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->FreeContext(context);
  }

  static void SetMaxAsyncTaskStacks(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr, "Internal error: setMaxAsyncTaskStacks(max).");
      Exit();
    }
    v8_inspector::SetMaxAsyncTaskStacksForTest(
        IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
            ->inspector()
            ->inspector(),
        args[0].As<v8::Int32>()->Value());
  }

  static void DumpAsyncTaskStacksStateForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: dumpAsyncTaskStacksStateForTest().");
      Exit();
    }
    v8_inspector::DumpAsyncTaskStacksStateForTest(
        IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
            ->inspector()
            ->inspector());
  }

  static void BreakProgram(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      fprintf(stderr, "Internal error: breakProgram('reason', 'details').");
      Exit();
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    v8::internal::Vector<uint16_t> reason = ToVector(args[0].As<v8::String>());
    v8_inspector::StringView reason_view(reason.start(), reason.length());
    v8::internal::Vector<uint16_t> details = ToVector(args[1].As<v8::String>());
    v8_inspector::StringView details_view(details.start(), details.length());
    data->inspector()->BreakProgram(data->GetContextGroupId(context),
                                    reason_view, details_view);
  }

  static void CreateObjectWithStrictCheck(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: createObjectWithStrictCheck().");
      Exit();
    }
    v8::Local<v8::ObjectTemplate> templ =
        v8::ObjectTemplate::New(args.GetIsolate());
    templ->SetAccessCheckCallback(&StrictAccessCheck);
    args.GetReturnValue().Set(
        templ->NewInstance(args.GetIsolate()->GetCurrentContext())
            .ToLocalChecked());
  }

  static void CallWithScheduledBreak(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsFunction() || !args[1]->IsString() ||
        !args[2]->IsString()) {
      fprintf(stderr,
              "Internal error: callWithScheduledBreak('reason', 'details').");
      Exit();
    }
    v8::internal::Vector<uint16_t> reason = ToVector(args[1].As<v8::String>());
    v8_inspector::StringView reason_view(reason.start(), reason.length());
    v8::internal::Vector<uint16_t> details = ToVector(args[2].As<v8::String>());
    v8_inspector::StringView details_view(details.start(), details.length());
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    int context_group_id = data->GetContextGroupId(context);
    data->inspector()->SchedulePauseOnNextStatement(context_group_id,
                                                    reason_view, details_view);
    v8::MaybeLocal<v8::Value> result;
    result = args[0].As<v8::Function>()->Call(context, context->Global(), 0,
                                              nullptr);
    data->inspector()->CancelPauseOnNextStatement(context_group_id);
  }

  static void AllowAccessorFormatting(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsObject()) {
      fprintf(stderr, "Internal error: allowAccessorFormatting('object').");
      Exit();
    }
    v8::Local<v8::Object> object = args[0].As<v8::Object>();
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Private> shouldFormatAccessorsPrivate = v8::Private::ForApi(
        isolate, v8::String::NewFromUtf8(isolate, "allowAccessorFormatting",
                                         v8::NewStringType::kNormal)
                     .ToLocalChecked());
    object
        ->SetPrivate(isolate->GetCurrentContext(), shouldFormatAccessorsPrivate,
                     v8::Null(isolate))
        .ToChecked();
  }
};

class FrontendChannelImpl : public InspectorClientImpl::FrontendChannel {
 public:
  FrontendChannelImpl(TaskRunner* frontend_task_runner, int context_group_id)
      : frontend_task_runner_(frontend_task_runner),
        context_group_id_(context_group_id) {}
  virtual ~FrontendChannelImpl() {}

  void SendMessageToFrontend(int session_id,
                             const v8_inspector::StringView& message) final {
    frontend_task_runner_->Append(new SendMessageToFrontendTask(
        context_group_id_, session_id, ToVector(message)));
  }

 private:
  TaskRunner* frontend_task_runner_;
  int context_group_id_;
};

}  //  namespace

int main(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::InitializeExternalStartupData(argv[0]);
  v8::V8::Initialize();

  v8::base::Semaphore ready_semaphore(0);

  v8::StartupData startup_data = {nullptr, 0};
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--embed") == 0) {
      argv[i++] = nullptr;
      printf("Embedding script '%s'\n", argv[i]);
      startup_data = v8::V8::CreateSnapshotDataBlob(argv[i]);
      argv[i] = nullptr;
    }
  }

  IsolateData::SetupGlobalTasks frontend_extensions;
  frontend_extensions.emplace_back(new UtilsExtension());
  TaskRunner frontend_runner(std::move(frontend_extensions), true,
                             &ready_semaphore, nullptr, nullptr);
  ready_semaphore.Wait();

  int frontend_context_group_id = 0;
  frontend_runner.Append(
      new CreateContextGroupTask(&ready_semaphore, &frontend_context_group_id));
  ready_semaphore.Wait();

  IsolateData::SetupGlobalTasks backend_extensions;
  backend_extensions.emplace_back(new SetTimeoutExtension());
  backend_extensions.emplace_back(new InspectorExtension());
  FrontendChannelImpl frontend_channel(&frontend_runner,
                                       frontend_context_group_id);
  TaskRunner backend_runner(
      std::move(backend_extensions), false, &ready_semaphore,
      startup_data.data ? &startup_data : nullptr, &frontend_channel);
  ready_semaphore.Wait();
  UtilsExtension::set_backend_task_runner(&backend_runner);

  task_runners.push_back(&frontend_runner);
  task_runners.push_back(&backend_runner);

  for (int i = 1; i < argc; ++i) {
    // Ignore unknown flags.
    if (argv[i] == nullptr || argv[i][0] == '-') continue;

    bool exists = false;
    v8::internal::Vector<const char> chars =
        v8::internal::ReadFile(argv[i], &exists, true);
    if (!exists) {
      fprintf(stderr, "Internal error: script file doesn't exists: %s\n",
              argv[i]);
      Exit();
    }
    frontend_runner.Append(
        new ExecuteStringTask(chars, frontend_context_group_id));
  }

  frontend_runner.Join();
  backend_runner.Join();

  delete startup_data.data;
  return 0;
}
