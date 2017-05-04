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

class UtilsExtension : public TaskRunner::SetupGlobalTask {
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
    utils->Set(ToV8String(isolate, "reconnect"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Reconnect));
    utils->Set(ToV8String(isolate, "disconnect"),
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Disconnect));
    utils->Set(ToV8String(isolate, "setLogConsoleApiMessageCalls"),
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetLogConsoleApiMessageCalls));
    utils->Set(ToV8String(isolate, "createContextGroup"),
               v8::FunctionTemplate::New(isolate,
                                         &UtilsExtension::CreateContextGroup));
    global->Set(ToV8String(isolate, "utils"), utils);
  }

  static void set_backend_task_runner(TaskRunner* runner) {
    backend_runner_ = runner;
  }

  static void set_inspector_client(InspectorClientImpl* client) {
    inspector_client_ = client;
  }

 private:
  static TaskRunner* backend_runner_;
  static InspectorClientImpl* inspector_client_;

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
    if (ReadFile(isolate, args[0], &chars)) {
      ExecuteStringTask task(chars);
      v8::Global<v8::Context> context(isolate, isolate->GetCurrentContext());
      task.Run(isolate, context);
    }
  }

  static void CompileAndRunWithOrigin(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 5 || !args[0]->IsString() || !args[1]->IsString() ||
        !args[2]->IsInt32() || !args[3]->IsInt32() || !args[4]->IsBoolean()) {
      fprintf(stderr,
              "Internal error: compileAndRunWithOrigin(source, name, line, "
              "column, is_module).");
      Exit();
    }

    backend_runner_->Append(new ExecuteStringTask(
        ToVector(args[0].As<v8::String>()), args[1].As<v8::String>(),
        args[2].As<v8::Int32>(), args[3].As<v8::Int32>(),
        args[4].As<v8::Boolean>(), nullptr, nullptr));
  }

  static void SetCurrentTimeMSForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsNumber()) {
      fprintf(stderr, "Internal error: setCurrentTimeMSForTest(time).");
      Exit();
    }
    inspector_client_->setCurrentTimeMSForTest(
        args[0].As<v8::Number>()->Value());
  }

  static void SetMemoryInfoForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1) {
      fprintf(stderr, "Internal error: setMemoryInfoForTest(value).");
      Exit();
    }
    inspector_client_->setMemoryInfoForTest(args[0]);
  }

  static void SchedulePauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      fprintf(
          stderr,
          "Internal error: schedulePauseOnNextStatement('reason', 'details').");
      Exit();
    }
    v8::internal::Vector<uint16_t> reason = ToVector(args[0].As<v8::String>());
    v8_inspector::StringView reason_view(reason.start(), reason.length());
    v8::internal::Vector<uint16_t> details = ToVector(args[1].As<v8::String>());
    v8_inspector::StringView details_view(details.start(), details.length());
    inspector_client_->session()->schedulePauseOnNextStatement(reason_view,
                                                               details_view);
  }

  static void CancelPauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: cancelPauseOnNextStatement().");
      Exit();
    }
    inspector_client_->session()->cancelPauseOnNextStatement();
  }

  static void Reconnect(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: reconnect().");
      Exit();
    }
    v8::base::Semaphore ready_semaphore(0);
    inspector_client_->scheduleReconnect(&ready_semaphore);
    ready_semaphore.Wait();
  }

  static void Disconnect(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: disconnect().");
      Exit();
    }
    v8::base::Semaphore ready_semaphore(0);
    inspector_client_->scheduleDisconnect(&ready_semaphore);
    ready_semaphore.Wait();
  }

  static void SetLogConsoleApiMessageCalls(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      fprintf(stderr, "Internal error: setLogConsoleApiMessageCalls(bool).");
      Exit();
    }
    inspector_client_->setLogConsoleApiMessageCalls(
        args[0].As<v8::Boolean>()->Value());
  }

  static void CreateContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

TaskRunner* UtilsExtension::backend_runner_ = nullptr;
InspectorClientImpl* UtilsExtension::inspector_client_ = nullptr;

class SetTimeoutTask : public AsyncTask {
 public:
  SetTimeoutTask(v8::Isolate* isolate, v8::Local<v8::Function> function,
                 const char* task_name, v8_inspector::V8Inspector* inspector)
      : AsyncTask(task_name, inspector), function_(isolate, function) {}
  virtual ~SetTimeoutTask() {}

  bool is_inspector_task() final { return false; }

  void AsyncRun(v8::Isolate* isolate,
                const v8::Global<v8::Context>& global_context) override {
    v8::MicrotasksScope microtasks_scope(isolate,
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = global_context.Get(isolate);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Function> function = function_.Get(isolate);
    v8::MaybeLocal<v8::Value> result;
    result = function->Call(context, context->Global(), 0, nullptr);
  }

 private:
  v8::Global<v8::Function> function_;
};

class SetTimeoutExtension : public TaskRunner::SetupGlobalTask {
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
      fprintf(stderr,
              "Internal error: only setTimeout(function, 0) is supported.");
      Exit();
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    std::unique_ptr<TaskRunner::Task> task;
    v8_inspector::V8Inspector* inspector =
        InspectorClientImpl::InspectorFromContext(context);
    if (args[0]->IsFunction()) {
      task.reset(new SetTimeoutTask(isolate,
                                    v8::Local<v8::Function>::Cast(args[0]),
                                    "setTimeout", inspector));
    } else {
      task.reset(new ExecuteStringTask(
          ToVector(args[0].As<v8::String>()), v8::String::Empty(isolate),
          v8::Integer::New(isolate, 0), v8::Integer::New(isolate, 0),
          v8::Boolean::New(isolate, false), "setTimeout", inspector));
    }
    TaskRunner::FromContext(context)->Append(task.release());
  }
};

bool StrictAccessCheck(v8::Local<v8::Context> accessing_context,
                       v8::Local<v8::Object> accessed_object,
                       v8::Local<v8::Value> data) {
  CHECK(accessing_context.IsEmpty());
  return accessing_context.IsEmpty();
}

class InspectorExtension : public TaskRunner::SetupGlobalTask {
 public:
  ~InspectorExtension() override = default;
  void Run(v8::Isolate* isolate,
           v8::Local<v8::ObjectTemplate> global) override {
    v8::Local<v8::ObjectTemplate> inspector = v8::ObjectTemplate::New(isolate);
    inspector->Set(
        ToV8String(isolate, "attachInspector"),
        v8::FunctionTemplate::New(isolate, &InspectorExtension::Attach));
    inspector->Set(
        ToV8String(isolate, "detachInspector"),
        v8::FunctionTemplate::New(isolate, &InspectorExtension::Detach));
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
  static void Attach(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8_inspector::V8Inspector* inspector =
        InspectorClientImpl::InspectorFromContext(context);
    if (!inspector) {
      fprintf(stderr, "Inspector client not found - cannot attach!");
      Exit();
    }
    inspector->contextCreated(
        v8_inspector::V8ContextInfo(context, 1, v8_inspector::StringView()));
  }

  static void Detach(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8_inspector::V8Inspector* inspector =
        InspectorClientImpl::InspectorFromContext(context);
    if (!inspector) {
      fprintf(stderr, "Inspector client not found - cannot detach!");
      Exit();
    }
    inspector->contextDestroyed(context);
  }

  static void SetMaxAsyncTaskStacks(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      fprintf(stderr, "Internal error: setMaxAsyncTaskStacks(max).");
      Exit();
    }
    v8_inspector::V8Inspector* inspector =
        InspectorClientImpl::InspectorFromContext(
            args.GetIsolate()->GetCurrentContext());
    CHECK(inspector);
    v8_inspector::SetMaxAsyncTaskStacksForTest(
        inspector, args[0].As<v8::Int32>()->Value());
  }

  static void DumpAsyncTaskStacksStateForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      fprintf(stderr, "Internal error: dumpAsyncTaskStacksStateForTest().");
      Exit();
    }
    v8_inspector::V8Inspector* inspector =
        InspectorClientImpl::InspectorFromContext(
            args.GetIsolate()->GetCurrentContext());
    CHECK(inspector);
    v8_inspector::DumpAsyncTaskStacksStateForTest(inspector);
  }

  static void BreakProgram(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      fprintf(stderr, "Internal error: breakProgram('reason', 'details').");
      Exit();
    }
    v8_inspector::V8InspectorSession* session =
        InspectorClientImpl::SessionFromContext(
            args.GetIsolate()->GetCurrentContext());
    CHECK(session);

    v8::internal::Vector<uint16_t> reason = ToVector(args[0].As<v8::String>());
    v8_inspector::StringView reason_view(reason.start(), reason.length());
    v8::internal::Vector<uint16_t> details = ToVector(args[1].As<v8::String>());
    v8_inspector::StringView details_view(details.start(), details.length());
    session->breakProgram(reason_view, details_view);
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
      fprintf(stderr, "Internal error: breakProgram('reason', 'details').");
      Exit();
    }
    v8_inspector::V8InspectorSession* session =
        InspectorClientImpl::SessionFromContext(
            args.GetIsolate()->GetCurrentContext());
    CHECK(session);

    v8::internal::Vector<uint16_t> reason = ToVector(args[1].As<v8::String>());
    v8_inspector::StringView reason_view(reason.start(), reason.length());
    v8::internal::Vector<uint16_t> details = ToVector(args[2].As<v8::String>());
    v8_inspector::StringView details_view(details.start(), details.length());
    session->schedulePauseOnNextStatement(reason_view, details_view);
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    v8::MaybeLocal<v8::Value> result;
    result = args[0].As<v8::Function>()->Call(context, context->Global(), 0,
                                              nullptr);
    session->cancelPauseOnNextStatement();
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

void UtilsExtension::CreateContextGroup(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 0) {
    fprintf(stderr, "Internal error: createContextGroup().");
    Exit();
  }
  v8::base::Semaphore ready_semaphore(0);
  int context_group_id = 0;
  TaskRunner::SetupGlobalTasks setup_global;
  setup_global.emplace_back(new SetTimeoutExtension());
  setup_global.emplace_back(new InspectorExtension());
  inspector_client_->scheduleCreateContextGroup(
      std::move(setup_global), &ready_semaphore, &context_group_id);
  ready_semaphore.Wait();
  args.GetReturnValue().Set(
      v8::Int32::New(args.GetIsolate(), context_group_id));
}

v8::Local<v8::String> ToString(v8::Isolate* isolate,
                               const v8_inspector::StringView& string) {
  if (string.is8Bit())
    return v8::String::NewFromOneByte(isolate, string.characters8(),
                                      v8::NewStringType::kNormal,
                                      static_cast<int>(string.length()))
        .ToLocalChecked();
  else
    return v8::String::NewFromTwoByte(isolate, string.characters16(),
                                      v8::NewStringType::kNormal,
                                      static_cast<int>(string.length()))
        .ToLocalChecked();
}

class FrontendChannelImpl : public InspectorClientImpl::FrontendChannel {
 public:
  explicit FrontendChannelImpl(TaskRunner* frontend_task_runner)
      : frontend_task_runner_(frontend_task_runner) {}
  virtual ~FrontendChannelImpl() {}

  void SendMessageToFrontend(const v8_inspector::StringView& message) final {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(v8::Isolate::GetCurrent());

    v8::Local<v8::String> prefix =
        v8::String::NewFromUtf8(isolate, "InspectorTest._dispatchMessage(",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked();
    v8::Local<v8::String> message_string = ToString(isolate, message);
    v8::Local<v8::String> suffix =
        v8::String::NewFromUtf8(isolate, ")", v8::NewStringType::kInternalized)
            .ToLocalChecked();

    v8::Local<v8::String> result = v8::String::Concat(prefix, message_string);
    result = v8::String::Concat(result, suffix);

    frontend_task_runner_->Append(new ExecuteStringTask(
        ToVector(result), v8::String::Empty(isolate),
        v8::Integer::New(isolate, 0), v8::Integer::New(isolate, 0),
        v8::Boolean::New(isolate, false), nullptr, nullptr));
  }

 private:
  TaskRunner* frontend_task_runner_;
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

  TaskRunner::SetupGlobalTasks backend_extensions;
  backend_extensions.emplace_back(new SetTimeoutExtension());
  backend_extensions.emplace_back(new InspectorExtension());
  TaskRunner backend_runner(std::move(backend_extensions), false,
                            &ready_semaphore,
                            startup_data.data ? &startup_data : nullptr);
  ready_semaphore.Wait();
  SendMessageToBackendExtension::set_backend_task_runner(&backend_runner);
  UtilsExtension::set_backend_task_runner(&backend_runner);

  TaskRunner::SetupGlobalTasks frontend_extensions;
  frontend_extensions.emplace_back(new UtilsExtension());
  frontend_extensions.emplace_back(new SendMessageToBackendExtension());
  TaskRunner frontend_runner(std::move(frontend_extensions), true,
                             &ready_semaphore, nullptr);
  ready_semaphore.Wait();

  FrontendChannelImpl frontend_channel(&frontend_runner);
  InspectorClientImpl inspector_client(&backend_runner, &frontend_channel,
                                       &ready_semaphore);
  ready_semaphore.Wait();
  UtilsExtension::set_inspector_client(&inspector_client);

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
    frontend_runner.Append(new ExecuteStringTask(chars));
  }

  frontend_runner.Join();
  backend_runner.Join();

  delete startup_data.data;
  return 0;
}
