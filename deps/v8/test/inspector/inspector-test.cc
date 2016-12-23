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

#include "test/inspector/inspector-impl.h"
#include "test/inspector/task-runner.h"

namespace {

void Exit() {
  fflush(stdout);
  fflush(stderr);
  _exit(0);
}

class UtilsExtension : public v8::Extension {
 public:
  UtilsExtension()
      : v8::Extension("v8_inspector/utils",
                      "native function print();"
                      "native function quit();"
                      "native function setlocale();") {}
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
    }
    return v8::Local<v8::FunctionTemplate>();
  }

 private:
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
};

class SetTimeoutTask : public TaskRunner::Task {
 public:
  SetTimeoutTask(v8::Isolate* isolate, v8::Local<v8::Function> function)
      : function_(isolate, function) {}
  virtual ~SetTimeoutTask() {}

  bool is_inspector_task() final { return false; }

  void Run(v8::Isolate* isolate,
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
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    if (args[0]->IsFunction()) {
      TaskRunner::FromContext(context)->Append(new SetTimeoutTask(
          args.GetIsolate(), v8::Local<v8::Function>::Cast(args[0])));
    } else {
      v8::Local<v8::String> data = args[0].As<v8::String>();
      std::unique_ptr<uint16_t[]> buffer(new uint16_t[data->Length()]);
      data->Write(reinterpret_cast<uint16_t*>(buffer.get()), 0, data->Length());
      v8_inspector::String16 source =
          v8_inspector::String16(buffer.get(), data->Length());
      TaskRunner::FromContext(context)->Append(new ExecuteStringTask(source));
    }
  }
};

v8_inspector::String16 ToString16(const v8_inspector::StringView& string) {
  if (string.is8Bit())
    return v8_inspector::String16(
        reinterpret_cast<const char*>(string.characters8()), string.length());
  return v8_inspector::String16(
      reinterpret_cast<const uint16_t*>(string.characters16()),
      string.length());
}

class FrontendChannelImpl : public InspectorClientImpl::FrontendChannel {
 public:
  explicit FrontendChannelImpl(TaskRunner* frontend_task_runner)
      : frontend_task_runner_(frontend_task_runner) {}
  virtual ~FrontendChannelImpl() {}

  void SendMessageToFrontend(const v8_inspector::StringView& message) final {
    v8_inspector::String16Builder script;
    script.append("InspectorTest._dispatchMessage(");
    script.append(ToString16(message));
    script.append(")");
    frontend_task_runner_->Append(new ExecuteStringTask(script.toString()));
  }

 private:
  TaskRunner* frontend_task_runner_;
};

}  //  namespace

int main(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::internal::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::InitializeExternalStartupData(argv[0]);
  v8::V8::Initialize();

  SetTimeoutExtension set_timeout_extension;
  v8::RegisterExtension(&set_timeout_extension);
  UtilsExtension utils_extension;
  v8::RegisterExtension(&utils_extension);
  SendMessageToBackendExtension send_message_to_backend_extension;
  v8::RegisterExtension(&send_message_to_backend_extension);

  v8::base::Semaphore ready_semaphore(0);

  const char* backend_extensions[] = {"v8_inspector/setTimeout"};
  v8::ExtensionConfiguration backend_configuration(
      arraysize(backend_extensions), backend_extensions);
  TaskRunner backend_runner(&backend_configuration, false, &ready_semaphore);
  ready_semaphore.Wait();
  SendMessageToBackendExtension::set_backend_task_runner(&backend_runner);

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
    v8_inspector::String16 source =
        v8_inspector::String16::fromUTF8(chars.start(), chars.length());
    frontend_runner.Append(new ExecuteStringTask(source));
  }

  frontend_runner.Join();
  return 0;
}
