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
#include "src/utils.h"
#include "src/vector.h"

#include "test/inspector/isolate-data.h"
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

v8::Local<v8::String> ToV8String(v8::Isolate* isolate, const char* str,
                                 int length) {
  return v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kNormal,
                                 length)
      .ToLocalChecked();
}

v8::Local<v8::String> ToV8String(v8::Isolate* isolate,
                                 const v8::internal::Vector<uint16_t>& buffer) {
  return v8::String::NewFromTwoByte(isolate, buffer.start(),
                                    v8::NewStringType::kNormal, buffer.length())
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

class FrontendChannelImpl : public v8_inspector::V8Inspector::Channel {
 public:
  FrontendChannelImpl(TaskRunner* task_runner, int context_group_id,
                      v8::Isolate* isolate, v8::Local<v8::Function> function)
      : task_runner_(task_runner),
        context_group_id_(context_group_id),
        function_(isolate, function) {}
  virtual ~FrontendChannelImpl() = default;

  void set_session_id(int session_id) { session_id_ = session_id; }

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    task_runner_->Append(
        new SendMessageTask(this, ToVector(message->string())));
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    task_runner_->Append(
        new SendMessageTask(this, ToVector(message->string())));
  }
  void flushProtocolNotifications() override {}

  class SendMessageTask : public TaskRunner::Task {
   public:
    SendMessageTask(FrontendChannelImpl* channel,
                    const v8::internal::Vector<uint16_t>& message)
        : channel_(channel), message_(message) {}
    virtual ~SendMessageTask() {}
    bool is_priority_task() final { return false; }

   private:
    void Run(IsolateData* data) override {
      v8::MicrotasksScope microtasks_scope(data->isolate(),
                                           v8::MicrotasksScope::kRunMicrotasks);
      v8::HandleScope handle_scope(data->isolate());
      v8::Local<v8::Context> context =
          data->GetContext(channel_->context_group_id_);
      v8::Context::Scope context_scope(context);
      v8::Local<v8::Value> message = ToV8String(data->isolate(), message_);
      v8::MaybeLocal<v8::Value> result;
      result = channel_->function_.Get(data->isolate())
                   ->Call(context, context->Global(), 1, &message);
    }
    FrontendChannelImpl* channel_;
    v8::internal::Vector<uint16_t> message_;
  };

  TaskRunner* task_runner_;
  int context_group_id_;
  v8::Global<v8::Function> function_;
  int session_id_;
  DISALLOW_COPY_AND_ASSIGN(FrontendChannelImpl);
};

template <typename T>
void RunSyncTask(TaskRunner* task_runner, T callback) {
  class SyncTask : public TaskRunner::Task {
   public:
    SyncTask(v8::base::Semaphore* ready_semaphore, T callback)
        : ready_semaphore_(ready_semaphore), callback_(callback) {}
    virtual ~SyncTask() = default;
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
  task_runner->Append(new SyncTask(&ready_semaphore, callback));
  ready_semaphore.Wait();
}

class SendMessageToBackendTask : public TaskRunner::Task {
 public:
  SendMessageToBackendTask(int session_id,
                           const v8::internal::Vector<uint16_t>& message)
      : session_id_(session_id), message_(message) {}
  bool is_priority_task() final { return true; }

 private:
  void Run(IsolateData* data) override {
    v8_inspector::StringView message_view(message_.start(), message_.length());
    data->SendMessage(session_id_, message_view);
  }

  int session_id_;
  v8::internal::Vector<uint16_t> message_;
};

void RunAsyncTask(TaskRunner* task_runner,
                  const v8_inspector::StringView& task_name,
                  TaskRunner::Task* task) {
  class AsyncTask : public TaskRunner::Task {
   public:
    explicit AsyncTask(TaskRunner::Task* inner) : inner_(inner) {}
    virtual ~AsyncTask() = default;
    bool is_priority_task() override { return inner_->is_priority_task(); }
    void Run(IsolateData* data) override {
      data->AsyncTaskStarted(inner_.get());
      inner_->Run(data);
      data->AsyncTaskFinished(inner_.get());
    }

   private:
    std::unique_ptr<TaskRunner::Task> inner_;
    DISALLOW_COPY_AND_ASSIGN(AsyncTask);
  };

  task_runner->data()->AsyncTaskScheduled(task_name, task, false);
  task_runner->Append(new AsyncTask(task));
}

class ExecuteStringTask : public TaskRunner::Task {
 public:
  ExecuteStringTask(int context_group_id,
                    const v8::internal::Vector<uint16_t>& expression,
                    v8::Local<v8::String> name,
                    v8::Local<v8::Integer> line_offset,
                    v8::Local<v8::Integer> column_offset,
                    v8::Local<v8::Boolean> is_module)
      : expression_(expression),
        name_(ToVector(name)),
        line_offset_(line_offset.As<v8::Int32>()->Value()),
        column_offset_(column_offset.As<v8::Int32>()->Value()),
        is_module_(is_module->Value()),
        context_group_id_(context_group_id) {}
  ExecuteStringTask(const v8::internal::Vector<const char>& expression,
                    int context_group_id)
      : expression_utf8_(expression), context_group_id_(context_group_id) {}
  bool is_priority_task() override { return false; }
  void Run(IsolateData* data) override {
    v8::MicrotasksScope microtasks_scope(data->isolate(),
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::HandleScope handle_scope(data->isolate());
    v8::Local<v8::Context> context = data->GetContext(context_group_id_);
    v8::Context::Scope context_scope(context);
    v8::ScriptOrigin origin(
        ToV8String(data->isolate(), name_),
        v8::Integer::New(data->isolate(), line_offset_),
        v8::Integer::New(data->isolate(), column_offset_),
        /* resource_is_shared_cross_origin */ v8::Local<v8::Boolean>(),
        /* script_id */ v8::Local<v8::Integer>(),
        /* source_map_url */ v8::Local<v8::Value>(),
        /* resource_is_opaque */ v8::Local<v8::Boolean>(),
        /* is_wasm */ v8::Local<v8::Boolean>(),
        v8::Boolean::New(data->isolate(), is_module_));
    v8::Local<v8::String> source;
    if (expression_.length())
      source = ToV8String(data->isolate(), expression_);
    else
      source = ToV8String(data->isolate(), expression_utf8_.start(),
                          expression_utf8_.length());

    v8::ScriptCompiler::Source scriptSource(source, origin);
    if (!is_module_) {
      v8::Local<v8::Script> script;
      if (!v8::ScriptCompiler::Compile(context, &scriptSource).ToLocal(&script))
        return;
      v8::MaybeLocal<v8::Value> result;
      result = script->Run(context);
    } else {
      data->RegisterModule(context, name_, &scriptSource);
    }
  }

 private:
  v8::internal::Vector<uint16_t> expression_;
  v8::internal::Vector<const char> expression_utf8_;
  v8::internal::Vector<uint16_t> name_;
  int32_t line_offset_ = 0;
  int32_t column_offset_ = 0;
  bool is_module_ = false;
  int context_group_id_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteStringTask);
};

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
    utils->Set(
        ToV8String(isolate, "setLogMaxAsyncCallStackDepthChanged"),
        v8::FunctionTemplate::New(
            isolate, &UtilsExtension::SetLogMaxAsyncCallStackDepthChanged));
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

  static void ClearAllSessions() { channels_.clear(); }

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

      v8::String::Utf8Value str(args.GetIsolate(), str_obj);
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

    v8::String::Utf8Value str(args.GetIsolate(), args[1]);
    setlocale(LC_NUMERIC, *str);
  }

  static bool ReadFile(v8::Isolate* isolate, v8::Local<v8::Value> name,
                       v8::internal::Vector<const char>* chars) {
    v8::String::Utf8Value str(isolate, name);
    bool exists = false;
    std::string filename(*str, str.length());
    *chars = v8::internal::ReadFile(filename.c_str(), &exists);
    if (!exists) {
      isolate->ThrowException(ToV8String(isolate, "Error reading file"));
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
    if (ReadFile(isolate, args[0], &chars))
      args.GetReturnValue().Set(ToV8String(isolate, chars.start()));
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
      ExecuteStringTask(chars, context_group_id).Run(data);
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
        args[0].As<v8::Int32>()->Value(), ToVector(args[1].As<v8::String>()),
        args[2].As<v8::String>(), args[3].As<v8::Int32>(),
        args[4].As<v8::Int32>(), args[5].As<v8::Boolean>()));
  }

  static void SetCurrentTimeMSForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsNumber()) {
      fprintf(stderr, "Internal error: setCurrentTimeMSForTest(time).");
      Exit();
    }
    backend_runner_->data()->SetCurrentTimeMS(
        args[0].As<v8::Number>()->Value());
  }

  static void SetMemoryInfoForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1) {
      fprintf(stderr, "Internal error: setMemoryInfoForTest(value).");
      Exit();
    }
    backend_runner_->data()->SetMemoryInfo(args[0]);
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
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_, [&context_group_id, &reason,
                                  &details](IsolateData* data) {
      data->SchedulePauseOnNextStatement(
          context_group_id,
          v8_inspector::StringView(reason.start(), reason.length()),
          v8_inspector::StringView(details.start(), details.length()));
    });
  }

  static void CancelPauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr,
              "Internal error: cancelPauseOnNextStatement(context_group_id).");
      Exit();
    }
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      data->CancelPauseOnNextStatement(context_group_id);
    });
  }

  static void SetLogConsoleApiMessageCalls(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      fprintf(stderr, "Internal error: setLogConsoleApiMessageCalls(bool).");
      Exit();
    }
    backend_runner_->data()->SetLogConsoleApiMessageCalls(
        args[0].As<v8::Boolean>()->Value());
  }

  static void SetLogMaxAsyncCallStackDepthChanged(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      fprintf(stderr,
              "Internal error: setLogMaxAsyncCallStackDepthChanged(bool).");
      Exit();
    }
    backend_runner_->data()->SetLogMaxAsyncCallStackDepthChanged(
        args[0].As<v8::Boolean>()->Value());
  }

  static void CreateContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: createContextGroup().");
      Exit();
    }
    int context_group_id = 0;
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      context_group_id = data->CreateContextGroup();
    });
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
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    FrontendChannelImpl* channel = new FrontendChannelImpl(
        IsolateData::FromContext(context)->task_runner(),
        IsolateData::FromContext(context)->GetContextGroupId(context),
        args.GetIsolate(), args[2].As<v8::Function>());

    v8::internal::Vector<uint16_t> state = ToVector(args[1].As<v8::String>());
    int context_group_id = args[0].As<v8::Int32>()->Value();
    int session_id = 0;
    RunSyncTask(backend_runner_, [&context_group_id, &session_id, &channel,
                                  &state](IsolateData* data) {
      session_id = data->ConnectSession(
          context_group_id,
          v8_inspector::StringView(state.start(), state.length()), channel);
      channel->set_session_id(session_id);
    });

    channels_[session_id].reset(channel);
    args.GetReturnValue().Set(v8::Int32::New(args.GetIsolate(), session_id));
  }

  static void DisconnectSession(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr, "Internal error: disconnectionSession(session_id).");
      Exit();
    }
    int session_id = args[0].As<v8::Int32>()->Value();
    v8::internal::Vector<uint16_t> state;
    RunSyncTask(backend_runner_, [&session_id, &state](IsolateData* data) {
      state = ToVector(data->DisconnectSession(session_id)->string());
    });
    channels_.erase(session_id);
    args.GetReturnValue().Set(ToV8String(args.GetIsolate(), state));
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

  static std::map<int, std::unique_ptr<FrontendChannelImpl>> channels_;
};

TaskRunner* UtilsExtension::backend_runner_ = nullptr;
std::map<int, std::unique_ptr<FrontendChannelImpl>> UtilsExtension::channels_;

class SetTimeoutTask : public TaskRunner::Task {
 public:
  SetTimeoutTask(int context_group_id, v8::Isolate* isolate,
                 v8::Local<v8::Function> function)
      : function_(isolate, function), context_group_id_(context_group_id) {}
  virtual ~SetTimeoutTask() {}
  bool is_priority_task() final { return false; }

 private:
  void Run(IsolateData* data) override {
    v8::MicrotasksScope microtasks_scope(data->isolate(),
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::HandleScope handle_scope(data->isolate());
    v8::Local<v8::Context> context = data->GetContext(context_group_id_);
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
      fprintf(
          stderr,
          "Internal error: only setTimeout(function|code, 0) is supported.");
      Exit();
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
                   new SetTimeoutTask(context_group_id, isolate,
                                      v8::Local<v8::Function>::Cast(args[0])));
    } else {
      RunAsyncTask(
          data->task_runner(), task_name_view,
          new ExecuteStringTask(
              context_group_id, ToVector(args[0].As<v8::String>()),
              v8::String::Empty(isolate), v8::Integer::New(isolate, 0),
              v8::Integer::New(isolate, 0), v8::Boolean::New(isolate, false)));
    }
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
    inspector->Set(ToV8String(isolate, "addInspectedObject"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::AddInspectedObject));
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
    inspector->Set(
        ToV8String(isolate, "markObjectAsNotInspectable"),
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::MarkObjectAsNotInspectable));
    inspector->Set(ToV8String(isolate, "createObjectWithAccessor"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::CreateObjectWithAccessor));
    inspector->Set(ToV8String(isolate, "storeCurrentStackTrace"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::StoreCurrentStackTrace));
    inspector->Set(ToV8String(isolate, "externalAsyncTaskStarted"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::ExternalAsyncTaskStarted));
    inspector->Set(
        ToV8String(isolate, "externalAsyncTaskFinished"),
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::ExternalAsyncTaskFinished));
    inspector->Set(ToV8String(isolate, "scheduleWithAsyncStack"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::ScheduleWithAsyncStack));
    global->Set(ToV8String(isolate, "inspector"), inspector);
  }

 private:
  static void FireContextCreated(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->FireContextCreated(context, data->GetContextGroupId(context));
  }

  static void FireContextDestroyed(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->FireContextDestroyed(context);
  }

  static void FreeContext(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->FreeContext(context);
  }

  static void AddInspectedObject(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsInt32()) {
      fprintf(stderr,
              "Internal error: addInspectedObject(session_id, object).");
      Exit();
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->AddInspectedObject(args[0].As<v8::Int32>()->Value(), args[1]);
  }

  static void SetMaxAsyncTaskStacks(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr, "Internal error: setMaxAsyncTaskStacks(max).");
      Exit();
    }
    IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
        ->SetMaxAsyncTaskStacksForTest(args[0].As<v8::Int32>()->Value());
  }

  static void DumpAsyncTaskStacksStateForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: dumpAsyncTaskStacksStateForTest().");
      Exit();
    }
    IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
        ->DumpAsyncTaskStacksStateForTest();
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
    data->BreakProgram(data->GetContextGroupId(context), reason_view,
                       details_view);
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
    data->SchedulePauseOnNextStatement(context_group_id, reason_view,
                                       details_view);
    v8::MaybeLocal<v8::Value> result;
    result = args[0].As<v8::Function>()->Call(context, context->Global(), 0,
                                              nullptr);
    data->CancelPauseOnNextStatement(context_group_id);
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
        isolate, ToV8String(isolate, "allowAccessorFormatting"));
    object
        ->SetPrivate(isolate->GetCurrentContext(), shouldFormatAccessorsPrivate,
                     v8::Null(isolate))
        .ToChecked();
  }

  static void MarkObjectAsNotInspectable(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsObject()) {
      fprintf(stderr, "Internal error: markObjectAsNotInspectable(object).");
      Exit();
    }
    v8::Local<v8::Object> object = args[0].As<v8::Object>();
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Private> notInspectablePrivate =
        v8::Private::ForApi(isolate, ToV8String(isolate, "notInspectable"));
    object
        ->SetPrivate(isolate->GetCurrentContext(), notInspectablePrivate,
                     v8::True(isolate))
        .ToChecked();
  }

  static void CreateObjectWithAccessor(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsBoolean()) {
      fprintf(stderr,
              "Internal error: createObjectWithAccessor('accessor name', "
              "hasSetter)\n");
      Exit();
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
    if (args[1].As<v8::Boolean>()->Value()) {
      templ->SetAccessor(v8::Local<v8::String>::Cast(args[0]), AccessorGetter,
                         AccessorSetter);
    } else {
      templ->SetAccessor(v8::Local<v8::String>::Cast(args[0]), AccessorGetter);
    }
    args.GetReturnValue().Set(
        templ->NewInstance(isolate->GetCurrentContext()).ToLocalChecked());
  }

  static void AccessorGetter(v8::Local<v8::String> property,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    isolate->ThrowException(ToV8String(isolate, "Getter is called"));
  }

  static void AccessorSetter(v8::Local<v8::String> property,
                             v8::Local<v8::Value> value,
                             const v8::PropertyCallbackInfo<void>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    isolate->ThrowException(ToV8String(isolate, "Setter is called"));
  }

  static void StoreCurrentStackTrace(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      fprintf(stderr,
              "Internal error: storeCurrentStackTrace('description')\n");
      Exit();
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    v8::internal::Vector<uint16_t> description =
        ToVector(args[0].As<v8::String>());
    v8_inspector::StringView description_view(description.start(),
                                              description.length());
    v8_inspector::V8StackTraceId id =
        data->StoreCurrentStackTrace(description_view);
    v8::Local<v8::ArrayBuffer> buffer =
        v8::ArrayBuffer::New(isolate, sizeof(id));
    *static_cast<v8_inspector::V8StackTraceId*>(buffer->GetContents().Data()) =
        id;
    args.GetReturnValue().Set(buffer);
  }

  static void ExternalAsyncTaskStarted(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsArrayBuffer()) {
      fprintf(stderr, "Internal error: externalAsyncTaskStarted(id)\n");
      Exit();
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    v8_inspector::V8StackTraceId* id =
        static_cast<v8_inspector::V8StackTraceId*>(
            args[0].As<v8::ArrayBuffer>()->GetContents().Data());
    data->ExternalAsyncTaskStarted(*id);
  }

  static void ExternalAsyncTaskFinished(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsArrayBuffer()) {
      fprintf(stderr, "Internal error: externalAsyncTaskFinished(id)\n");
      Exit();
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    v8_inspector::V8StackTraceId* id =
        static_cast<v8_inspector::V8StackTraceId*>(
            args[0].As<v8::ArrayBuffer>()->GetContents().Data());
    data->ExternalAsyncTaskFinished(*id);
  }

  static void ScheduleWithAsyncStack(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsFunction() || !args[1]->IsString() ||
        !args[2]->IsBoolean()) {
      fprintf(stderr,
              "Internal error: scheduleWithAsyncStack(function, "
              "'task-name', with_empty_stack).");
      Exit();
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    int context_group_id = data->GetContextGroupId(context);
    bool with_empty_stack = args[2].As<v8::Boolean>()->Value();
    if (with_empty_stack) context->Exit();

    v8::internal::Vector<uint16_t> task_name =
        ToVector(args[1].As<v8::String>());
    v8_inspector::StringView task_name_view(task_name.start(),
                                            task_name.length());

    RunAsyncTask(data->task_runner(), task_name_view,
                 new SetTimeoutTask(context_group_id, isolate,
                                    v8::Local<v8::Function>::Cast(args[0])));
    if (with_empty_stack) context->Enter();
  }
};

}  //  namespace

int main(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  std::unique_ptr<v8::Platform> platform(v8::platform::NewDefaultPlatform());
  v8::V8::InitializePlatform(platform.get());
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
                             &ready_semaphore, nullptr, false);
  ready_semaphore.Wait();

  int frontend_context_group_id = 0;
  RunSyncTask(&frontend_runner,
              [&frontend_context_group_id](IsolateData* data) {
                frontend_context_group_id = data->CreateContextGroup();
              });

  IsolateData::SetupGlobalTasks backend_extensions;
  backend_extensions.emplace_back(new SetTimeoutExtension());
  backend_extensions.emplace_back(new InspectorExtension());
  TaskRunner backend_runner(std::move(backend_extensions), false,
                            &ready_semaphore,
                            startup_data.data ? &startup_data : nullptr, true);
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
  UtilsExtension::ClearAllSessions();
  return 0;
}
