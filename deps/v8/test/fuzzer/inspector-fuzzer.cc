// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif               // !defined(_WIN32) && !defined(_WIN64)

#include <locale.h>

#include <string>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/small-vector.h"
#include "src/flags/flags.h"
#include "src/heap/read-only-heap.h"
#include "src/libplatform/default-platform.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "test/inspector/frontend-channel.h"
#include "test/inspector/isolate-data.h"
#include "test/inspector/task-runner.h"
#include "test/inspector/tasks.h"

namespace v8 {
namespace internal {
namespace {

base::SmallVector<TaskRunner*, 2> task_runners;

class UtilsExtension : public IsolateData::SetupGlobalTask {
 public:
  ~UtilsExtension() override = default;
  void Run(v8::Isolate* isolate,
           v8::Local<v8::ObjectTemplate> global) override {
    v8::Local<v8::ObjectTemplate> utils = v8::ObjectTemplate::New(isolate);
    auto Set = [isolate](v8::Local<v8::ObjectTemplate> tmpl, const char* str,
                         v8::Local<v8::Data> value) {
      // Do not set {ReadOnly}, because fuzzer inputs might overwrite individual
      // methods, or the whole "utils" global. See the
      // `testing/libfuzzer/fuzzers/generate_v8_inspector_fuzzer_corpus.py` file
      // in chromium.
      tmpl->Set(ToV8String(isolate, str), value,
                static_cast<v8::PropertyAttribute>(
                    v8::PropertyAttribute::DontDelete));
    };
    Set(utils, "quit",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::Quit));
    Set(utils, "compileAndRunWithOrigin",
        v8::FunctionTemplate::New(isolate,
                                  &UtilsExtension::CompileAndRunWithOrigin));
    Set(utils, "schedulePauseOnNextStatement",
        v8::FunctionTemplate::New(
            isolate, &UtilsExtension::SchedulePauseOnNextStatement));
    Set(utils, "cancelPauseOnNextStatement",
        v8::FunctionTemplate::New(isolate,
                                  &UtilsExtension::CancelPauseOnNextStatement));
    Set(utils, "createContextGroup",
        v8::FunctionTemplate::New(isolate,
                                  &UtilsExtension::CreateContextGroup));
    Set(utils, "resetContextGroup",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::ResetContextGroup));
    Set(utils, "connectSession",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::ConnectSession));
    Set(utils, "disconnectSession",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::DisconnectSession));
    Set(utils, "sendMessageToBackend",
        v8::FunctionTemplate::New(isolate,
                                  &UtilsExtension::SendMessageToBackend));
    Set(global, "utils", utils);
  }

  static void set_backend_task_runner(TaskRunner* runner) {
    backend_runner_ = runner;
  }

  static void ClearAllSessions() { channels_.clear(); }

 private:
  static TaskRunner* backend_runner_;

  static void Quit(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Only terminate, so not join the threads here, since joining concurrently
    // from multiple threads can be undefined behaviour (see pthread_join).
    for (TaskRunner* task_runner : task_runners) task_runner->Terminate();
  }

  static void CompileAndRunWithOrigin(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 6 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsString() || !args[3]->IsInt32() || !args[4]->IsInt32() ||
        !args[5]->IsBoolean()) {
      return;
    }

    backend_runner_->Append(std::make_unique<ExecuteStringTask>(
        args.GetIsolate(), args[0].As<v8::Int32>()->Value(),
        ToVector(args.GetIsolate(), args[1].As<v8::String>()),
        args[2].As<v8::String>(), args[3].As<v8::Int32>(),
        args[4].As<v8::Int32>(), args[5].As<v8::Boolean>()));
  }

  static void SchedulePauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsString()) {
      return;
    }
    std::vector<uint16_t> reason =
        ToVector(args.GetIsolate(), args[1].As<v8::String>());
    std::vector<uint16_t> details =
        ToVector(args.GetIsolate(), args[2].As<v8::String>());
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_,
                [&context_group_id, &reason, &details](IsolateData* data) {
                  data->SchedulePauseOnNextStatement(
                      context_group_id,
                      v8_inspector::StringView(reason.data(), reason.size()),
                      v8_inspector::StringView(details.data(), details.size()));
                });
  }

  static void CancelPauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      return;
    }
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      data->CancelPauseOnNextStatement(context_group_id);
    });
  }

  static void CreateContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      return;
    }
    int context_group_id = 0;
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      context_group_id = data->CreateContextGroup();
    });
    args.GetReturnValue().Set(
        v8::Int32::New(args.GetIsolate(), context_group_id));
  }

  static void ResetContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      return;
    }
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      data->ResetContextGroup(context_group_id);
    });
  }

  static void ConnectSession(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsFunction()) {
      return;
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    FrontendChannelImpl* channel = new FrontendChannelImpl(
        IsolateData::FromContext(context)->task_runner(),
        IsolateData::FromContext(context)->GetContextGroupId(context),
        args.GetIsolate(), args[2].As<v8::Function>());

    std::vector<uint8_t> state =
        ToBytes(args.GetIsolate(), args[1].As<v8::String>());
    int context_group_id = args[0].As<v8::Int32>()->Value();
    int session_id = 0;
    RunSyncTask(backend_runner_, [&context_group_id, &session_id, &channel,
                                  &state](IsolateData* data) {
      session_id = data->ConnectSession(
          context_group_id,
          v8_inspector::StringView(state.data(), state.size()), channel);
      channel->set_session_id(session_id);
    });

    channels_[session_id].reset(channel);
    args.GetReturnValue().Set(v8::Int32::New(args.GetIsolate(), session_id));
  }

  static void DisconnectSession(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      return;
    }
    int session_id = args[0].As<v8::Int32>()->Value();
    std::vector<uint8_t> state;
    RunSyncTask(backend_runner_, [&session_id, &state](IsolateData* data) {
      state = data->DisconnectSession(session_id);
    });
    channels_.erase(session_id);
    args.GetReturnValue().Set(ToV8String(args.GetIsolate(), state));
  }

  static void SendMessageToBackend(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsInt32() || !args[1]->IsString()) {
      return;
    }
    backend_runner_->Append(std::make_unique<SendMessageToBackendTask>(
        args[0].As<v8::Int32>()->Value(),
        ToVector(args.GetIsolate(), args[1].As<v8::String>())));
  }

  static std::map<int, std::unique_ptr<FrontendChannelImpl>> channels_;
};

TaskRunner* UtilsExtension::backend_runner_ = nullptr;
std::map<int, std::unique_ptr<FrontendChannelImpl>> UtilsExtension::channels_;

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
    inspector->Set(
        ToV8String(isolate, "setAllowCodeGenerationFromStrings"),
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::SetAllowCodeGenerationFromStrings));
    inspector->Set(ToV8String(isolate, "setResourceNamePrefix"),
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::SetResourceNamePrefix));
    global->Set(ToV8String(isolate, "inspector"), inspector);
  }

 private:
  static void FireContextCreated(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->FireContextCreated(context, data->GetContextGroupId(context),
                             v8_inspector::StringView());
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
      return;
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->AddInspectedObject(args[0].As<v8::Int32>()->Value(), args[1]);
  }

  static void SetMaxAsyncTaskStacks(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      return;
    }
    IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
        ->SetMaxAsyncTaskStacksForTest(args[0].As<v8::Int32>()->Value());
  }

  static void DumpAsyncTaskStacksStateForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      return;
    }
    IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
        ->DumpAsyncTaskStacksStateForTest();
  }

  static void BreakProgram(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      return;
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    std::vector<uint16_t> reason =
        ToVector(args.GetIsolate(), args[0].As<v8::String>());
    v8_inspector::StringView reason_view(reason.data(), reason.size());
    std::vector<uint16_t> details =
        ToVector(args.GetIsolate(), args[1].As<v8::String>());
    v8_inspector::StringView details_view(details.data(), details.size());
    data->BreakProgram(data->GetContextGroupId(context), reason_view,
                       details_view);
  }

  static void CreateObjectWithStrictCheck(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      return;
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
      return;
    }
    std::vector<uint16_t> reason =
        ToVector(args.GetIsolate(), args[1].As<v8::String>());
    v8_inspector::StringView reason_view(reason.data(), reason.size());
    std::vector<uint16_t> details =
        ToVector(args.GetIsolate(), args[2].As<v8::String>());
    v8_inspector::StringView details_view(details.data(), details.size());
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
      return;
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
      return;
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
      return;
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
      return;
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    std::vector<uint16_t> description =
        ToVector(isolate, args[0].As<v8::String>());
    v8_inspector::StringView description_view(description.data(),
                                              description.size());
    v8_inspector::V8StackTraceId id =
        data->StoreCurrentStackTrace(description_view);
    v8::Local<v8::ArrayBuffer> buffer =
        v8::ArrayBuffer::New(isolate, sizeof(id));
    *static_cast<v8_inspector::V8StackTraceId*>(
        buffer->GetBackingStore()->Data()) = id;
    args.GetReturnValue().Set(buffer);
  }

  static void ExternalAsyncTaskStarted(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsArrayBuffer()) {
      return;
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    v8_inspector::V8StackTraceId* id =
        static_cast<v8_inspector::V8StackTraceId*>(
            args[0].As<v8::ArrayBuffer>()->GetBackingStore()->Data());
    data->ExternalAsyncTaskStarted(*id);
  }

  static void ExternalAsyncTaskFinished(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsArrayBuffer()) {
      return;
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    v8_inspector::V8StackTraceId* id =
        static_cast<v8_inspector::V8StackTraceId*>(
            args[0].As<v8::ArrayBuffer>()->GetBackingStore()->Data());
    data->ExternalAsyncTaskFinished(*id);
  }

  static void ScheduleWithAsyncStack(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsFunction() || !args[1]->IsString() ||
        !args[2]->IsBoolean()) {
      return;
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    int context_group_id = data->GetContextGroupId(context);
    bool with_empty_stack = args[2].As<v8::Boolean>()->Value();
    if (with_empty_stack) context->Exit();

    std::vector<uint16_t> task_name =
        ToVector(isolate, args[1].As<v8::String>());
    v8_inspector::StringView task_name_view(task_name.data(), task_name.size());

    RunAsyncTask(
        data->task_runner(), task_name_view,
        std::make_unique<SetTimeoutTask>(
            context_group_id, isolate, v8::Local<v8::Function>::Cast(args[0])));
    if (with_empty_stack) context->Enter();
  }

  static void SetAllowCodeGenerationFromStrings(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      return;
    }
    args.GetIsolate()->GetCurrentContext()->AllowCodeGenerationFromStrings(
        args[0].As<v8::Boolean>()->Value());
  }

  static void SetResourceNamePrefix(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      return;
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->SetResourceNamePrefix(v8::Local<v8::String>::Cast(args[0]));
  }
};

using CharVector = v8::internal::Vector<const char>;

constexpr auto kMaxExecutionSeconds = v8::base::TimeDelta::FromSeconds(2);

class Watchdog final : public base::Thread {
 public:
  explicit Watchdog(base::Semaphore* semaphore)
      : base::Thread(base::Thread::Options("InspectorFuzzerWatchdog")),
        semaphore_(semaphore) {
    CHECK(Start());
  }

 private:
  void Run() override {
    if (semaphore_->WaitFor(kMaxExecutionSeconds)) return;
    for (TaskRunner* task_runner : task_runners) task_runner->Terminate();
  }

  base::Semaphore* const semaphore_;
};

void FuzzInspector(const uint8_t* data, size_t size) {
  base::Semaphore ready_semaphore(0);

  IsolateData::SetupGlobalTasks frontend_extensions;
  frontend_extensions.emplace_back(new UtilsExtension());
  TaskRunner frontend_runner(std::move(frontend_extensions),
                             kSuppressUncaughtExceptions, &ready_semaphore,
                             nullptr, kNoInspector);
  ready_semaphore.Wait();

  int frontend_context_group_id = 0;
  RunSyncTask(&frontend_runner,
              [&frontend_context_group_id](IsolateData* data) {
                frontend_context_group_id = data->CreateContextGroup();
              });

  IsolateData::SetupGlobalTasks backend_extensions;
  backend_extensions.emplace_back(new SetTimeoutExtension());
  backend_extensions.emplace_back(new InspectorExtension());
  TaskRunner backend_runner(std::move(backend_extensions),
                            kSuppressUncaughtExceptions, &ready_semaphore,
                            nullptr, kWithInspector);
  ready_semaphore.Wait();
  UtilsExtension::set_backend_task_runner(&backend_runner);

  task_runners = {&frontend_runner, &backend_runner};

  Watchdog watchdog(&ready_semaphore);

  frontend_runner.Append(std::make_unique<ExecuteStringTask>(
      std::string{reinterpret_cast<const char*>(data), size},
      frontend_context_group_id));

  frontend_runner.Join();
  backend_runner.Join();

  ready_semaphore.Signal();
  watchdog.Join();

  UtilsExtension::ClearAllSessions();

  // TaskRunners go out of scope here, which causes Isolate teardown and all
  // running background tasks to be properly joined.
}

}  //  namespace
}  // namespace internal
}  // namespace v8

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8::internal::FuzzInspector(data, size);
  return 0;
}
