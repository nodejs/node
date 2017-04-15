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

class UtilsExtension : public v8::Extension {
 public:
  UtilsExtension()
      : v8::Extension("v8_inspector/utils",
                      "native function print();"
                      "native function quit();"
                      "native function setlocale();"
                      "native function load();"
                      "native function compileAndRunWithOrigin();") {}
  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (name->Equals(context, v8::String::NewFromUtf8(
                                  isolate, "print", v8::NewStringType::kNormal)
                                  .ToLocalChecked())
            .FromJust()) {
      return v8::FunctionTemplate::New(isolate, UtilsExtension::Print);
    } else if (name->Equals(context,
                            v8::String::NewFromUtf8(isolate, "quit",
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked())
                   .FromJust()) {
      return v8::FunctionTemplate::New(isolate, UtilsExtension::Quit);
    } else if (name->Equals(context,
                            v8::String::NewFromUtf8(isolate, "setlocale",
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked())
                   .FromJust()) {
      return v8::FunctionTemplate::New(isolate, UtilsExtension::SetLocale);
    } else if (name->Equals(context,
                            v8::String::NewFromUtf8(isolate, "load",
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked())
                   .FromJust()) {
      return v8::FunctionTemplate::New(isolate, UtilsExtension::Load);
    } else if (name->Equals(context, v8::String::NewFromUtf8(
                                         isolate, "compileAndRunWithOrigin",
                                         v8::NewStringType::kNormal)
                                         .ToLocalChecked())
                   .FromJust()) {
      return v8::FunctionTemplate::New(isolate,
                                       UtilsExtension::CompileAndRunWithOrigin);
    }
    return v8::Local<v8::FunctionTemplate>();
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

  static void SetLocale(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      fprintf(stderr, "Internal error: setlocale get one string argument.");
      Exit();
    }
    v8::String::Utf8Value str(args[0]);
    setlocale(LC_NUMERIC, *str);
  }

  static void Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      fprintf(stderr, "Internal error: load gets one string argument.");
      Exit();
    }
    v8::String::Utf8Value str(args[0]);
    v8::Isolate* isolate = args.GetIsolate();
    bool exists = false;
    std::string filename(*str, str.length());
    v8::internal::Vector<const char> chars =
        v8::internal::ReadFile(filename.c_str(), &exists);
    if (!exists) {
      isolate->ThrowException(
          v8::String::NewFromUtf8(isolate, "Error loading file",
                                  v8::NewStringType::kNormal)
              .ToLocalChecked());
      return;
    }
    ExecuteStringTask task(chars);
    v8::Global<v8::Context> context(isolate, isolate->GetCurrentContext());
    task.Run(isolate, context);
  }

  static void CompileAndRunWithOrigin(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 4 || !args[0]->IsString() || !args[1]->IsString() ||
        !args[2]->IsInt32() || !args[3]->IsInt32()) {
      fprintf(stderr,
              "Internal error: compileAndRunWithOrigin(source, name, line, "
              "column).");
      Exit();
    }

    backend_runner_->Append(new ExecuteStringTask(
        ToVector(args[0].As<v8::String>()), args[1].As<v8::String>(),
        args[2].As<v8::Int32>(), args[3].As<v8::Int32>(), nullptr, nullptr));
  }
};

TaskRunner* UtilsExtension::backend_runner_ = nullptr;

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
    v8_inspector::V8Inspector* inspector =
        InspectorClientImpl::InspectorFromContext(context);
    if (inspector) inspector->willExecuteScript(context, function->ScriptId());
    result = function->Call(context, context->Global(), 0, nullptr);
    if (inspector) inspector->didExecuteScript(context);
  }

 private:
  v8::Global<v8::Function> function_;
};

class SetTimeoutExtension : public v8::Extension {
 public:
  SetTimeoutExtension()
      : v8::Extension("v8_inspector/setTimeout",
                      "native function setTimeout();") {}

  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) {
    return v8::FunctionTemplate::New(isolate, SetTimeoutExtension::SetTimeout);
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
          "setTimeout", inspector));
    }
    TaskRunner::FromContext(context)->Append(task.release());
  }
};

class InspectorExtension : public v8::Extension {
 public:
  InspectorExtension()
      : v8::Extension("v8_inspector/inspector",
                      "native function attachInspector();"
                      "native function detachInspector();"
                      "native function setMaxAsyncTaskStacks();") {}

  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (name->Equals(context,
                     v8::String::NewFromUtf8(isolate, "attachInspector",
                                             v8::NewStringType::kNormal)
                         .ToLocalChecked())
            .FromJust()) {
      return v8::FunctionTemplate::New(isolate, InspectorExtension::Attach);
    } else if (name->Equals(context,
                            v8::String::NewFromUtf8(isolate, "detachInspector",
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked())
                   .FromJust()) {
      return v8::FunctionTemplate::New(isolate, InspectorExtension::Detach);
    } else if (name->Equals(context, v8::String::NewFromUtf8(
                                         isolate, "setMaxAsyncTaskStacks",
                                         v8::NewStringType::kNormal)
                                         .ToLocalChecked())
                   .FromJust()) {
      return v8::FunctionTemplate::New(
          isolate, InspectorExtension::SetMaxAsyncTaskStacks);
    }
    return v8::Local<v8::FunctionTemplate>();
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
};

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

    frontend_task_runner_->Append(
        new ExecuteStringTask(ToVector(result), v8::String::Empty(isolate),
                              v8::Integer::New(isolate, 0),
                              v8::Integer::New(isolate, 0), nullptr, nullptr));
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

  SetTimeoutExtension set_timeout_extension;
  v8::RegisterExtension(&set_timeout_extension);
  InspectorExtension inspector_extension;
  v8::RegisterExtension(&inspector_extension);
  UtilsExtension utils_extension;
  v8::RegisterExtension(&utils_extension);
  SendMessageToBackendExtension send_message_to_backend_extension;
  v8::RegisterExtension(&send_message_to_backend_extension);

  v8::base::Semaphore ready_semaphore(0);

  const char* backend_extensions[] = {"v8_inspector/setTimeout",
                                      "v8_inspector/inspector"};
  v8::ExtensionConfiguration backend_configuration(
      arraysize(backend_extensions), backend_extensions);
  TaskRunner backend_runner(&backend_configuration, false, &ready_semaphore);
  ready_semaphore.Wait();
  SendMessageToBackendExtension::set_backend_task_runner(&backend_runner);
  UtilsExtension::set_backend_task_runner(&backend_runner);

  const char* frontend_extensions[] = {"v8_inspector/utils",
                                       "v8_inspector/frontend"};
  v8::ExtensionConfiguration frontend_configuration(
      arraysize(frontend_extensions), frontend_extensions);
  TaskRunner frontend_runner(&frontend_configuration, true, &ready_semaphore);
  ready_semaphore.Wait();

  FrontendChannelImpl frontend_channel(&frontend_runner);
  InspectorClientImpl inspector_client(&backend_runner, &frontend_channel,
                                       &ready_semaphore);
  ready_semaphore.Wait();

  task_runners.push_back(&frontend_runner);
  task_runners.push_back(&backend_runner);

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') break;

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
  return 0;
}
