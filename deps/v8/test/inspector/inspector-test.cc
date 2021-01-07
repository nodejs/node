// Copyright 2016 the V8 project authors. All rights reserved.
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
#include "src/base/small-vector.h"
#include "src/flags/flags.h"
#include "src/heap/read-only-heap.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "test/inspector/frontend-channel.h"
#include "test/inspector/isolate-data.h"
#include "test/inspector/task-runner.h"
#include "test/inspector/tasks.h"
#include "test/inspector/utils.h"

namespace v8 {
namespace internal {

extern void DisableEmbeddedBlobRefcounting();
extern void FreeCurrentEmbeddedBlob();

extern v8::StartupData CreateSnapshotDataBlobInternal(
    v8::SnapshotCreator::FunctionCodeHandling function_code_handling,
    const char* embedded_source, v8::Isolate* isolate);
extern v8::StartupData WarmUpSnapshotDataBlobInternal(
    v8::StartupData cold_snapshot_blob, const char* warmup_source);

namespace {

base::SmallVector<TaskRunner*, 2> task_runners;

class UtilsExtension : public IsolateData::SetupGlobalTask {
 public:
  ~UtilsExtension() override = default;
  void Run(v8::Isolate* isolate,
           v8::Local<v8::ObjectTemplate> global) override {
    v8::Local<v8::ObjectTemplate> utils = v8::ObjectTemplate::New(isolate);
    utils->Set(isolate, "print",
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Print));
    utils->Set(isolate, "quit",
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Quit));
    utils->Set(isolate, "setlocale",
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Setlocale));
    utils->Set(isolate, "read",
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Read));
    utils->Set(isolate, "load",
               v8::FunctionTemplate::New(isolate, &UtilsExtension::Load));
    utils->Set(isolate, "compileAndRunWithOrigin",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::CompileAndRunWithOrigin));
    utils->Set(isolate, "setCurrentTimeMSForTest",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetCurrentTimeMSForTest));
    utils->Set(isolate, "setMemoryInfoForTest",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetMemoryInfoForTest));
    utils->Set(isolate, "schedulePauseOnNextStatement",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SchedulePauseOnNextStatement));
    utils->Set(isolate, "cancelPauseOnNextStatement",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::CancelPauseOnNextStatement));
    utils->Set(isolate, "setLogConsoleApiMessageCalls",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetLogConsoleApiMessageCalls));
    utils->Set(isolate, "setAdditionalConsoleApi",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SetAdditionalConsoleApi));
    utils->Set(
        isolate, "setLogMaxAsyncCallStackDepthChanged",
        v8::FunctionTemplate::New(
            isolate, &UtilsExtension::SetLogMaxAsyncCallStackDepthChanged));
    utils->Set(isolate, "createContextGroup",
               v8::FunctionTemplate::New(isolate,
                                         &UtilsExtension::CreateContextGroup));
    utils->Set(
        isolate, "createContext",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::CreateContext));
    utils->Set(
        isolate, "resetContextGroup",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::ResetContextGroup));
    utils->Set(
        isolate, "connectSession",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::ConnectSession));
    utils->Set(
        isolate, "disconnectSession",
        v8::FunctionTemplate::New(isolate, &UtilsExtension::DisconnectSession));
    utils->Set(isolate, "sendMessageToBackend",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::SendMessageToBackend));
    utils->Set(isolate, "interruptForMessages",
               v8::FunctionTemplate::New(
                   isolate, &UtilsExtension::InterruptForMessages));
    global->Set(isolate, "utils", utils);
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
        arg = v8::Local<v8::Symbol>::Cast(arg)->Description();
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
        FATAL("Error in fwrite\n");
      }
    }
    printf("\n");
    fflush(stdout);
  }

  static void Quit(const v8::FunctionCallbackInfo<v8::Value>& args) {
    fflush(stdout);
    fflush(stderr);
    // Only terminate, so not join the threads here, since joining concurrently
    // from multiple threads can be undefined behaviour (see pthread_join).
    for (TaskRunner* task_runner : task_runners) task_runner->Terminate();
  }

  static void Setlocale(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      FATAL("Internal error: setlocale get one string argument.");
    }

    v8::String::Utf8Value str(args.GetIsolate(), args[1]);
    setlocale(LC_NUMERIC, *str);
  }

  static bool ReadFile(v8::Isolate* isolate, v8::Local<v8::Value> name,
                       std::string* chars) {
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
      FATAL("Internal error: read gets one string argument.");
    }
    std::string chars;
    v8::Isolate* isolate = args.GetIsolate();
    if (ReadFile(isolate, args[0], &chars)) {
      args.GetReturnValue().Set(ToV8String(isolate, chars));
    }
  }

  static void Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      FATAL("Internal error: load gets one string argument.");
    }
    std::string chars;
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
      FATAL(
          "Internal error: compileAndRunWithOrigin(context_group_id, source, "
          "name, line, column, is_module).");
    }

    backend_runner_->Append(std::make_unique<ExecuteStringTask>(
        args.GetIsolate(), args[0].As<v8::Int32>()->Value(),
        ToVector(args.GetIsolate(), args[1].As<v8::String>()),
        args[2].As<v8::String>(), args[3].As<v8::Int32>(),
        args[4].As<v8::Int32>(), args[5].As<v8::Boolean>()));
  }

  static void SetCurrentTimeMSForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsNumber()) {
      FATAL("Internal error: setCurrentTimeMSForTest(time).");
    }
    backend_runner_->data()->SetCurrentTimeMS(
        args[0].As<v8::Number>()->Value());
  }

  static void SetMemoryInfoForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1) {
      FATAL("Internal error: setMemoryInfoForTest(value).");
    }
    backend_runner_->data()->SetMemoryInfo(args[0]);
  }

  static void SchedulePauseOnNextStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsString()) {
      FATAL(
          "Internal error: schedulePauseOnNextStatement(context_group_id, "
          "'reason', 'details').");
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
      FATAL("Internal error: cancelPauseOnNextStatement(context_group_id).");
    }
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      data->CancelPauseOnNextStatement(context_group_id);
    });
  }

  static void SetLogConsoleApiMessageCalls(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      FATAL("Internal error: setLogConsoleApiMessageCalls(bool).");
    }
    backend_runner_->data()->SetLogConsoleApiMessageCalls(
        args[0].As<v8::Boolean>()->Value());
  }

  static void SetLogMaxAsyncCallStackDepthChanged(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsBoolean()) {
      FATAL("Internal error: setLogMaxAsyncCallStackDepthChanged(bool).");
    }
    backend_runner_->data()->SetLogMaxAsyncCallStackDepthChanged(
        args[0].As<v8::Boolean>()->Value());
  }

  static void SetAdditionalConsoleApi(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      FATAL("Internal error: SetAdditionalConsoleApi(string).");
    }
    std::vector<uint16_t> script =
        ToVector(args.GetIsolate(), args[0].As<v8::String>());
    RunSyncTask(backend_runner_, [&script](IsolateData* data) {
      data->SetAdditionalConsoleApi(
          v8_inspector::StringView(script.data(), script.size()));
    });
  }

  static void CreateContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      FATAL("Internal error: createContextGroup().");
    }
    int context_group_id = 0;
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      context_group_id = data->CreateContextGroup();
    });
    args.GetReturnValue().Set(
        v8::Int32::New(args.GetIsolate(), context_group_id));
  }

  static void CreateContext(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2) {
      FATAL("Internal error: createContext(context, name).");
    }
    int context_group_id = args[0].As<v8::Int32>()->Value();
    std::vector<uint16_t> name =
        ToVector(args.GetIsolate(), args[1].As<v8::String>());

    RunSyncTask(backend_runner_, [&context_group_id, name](IsolateData* data) {
      data->CreateContext(context_group_id,
                          v8_inspector::StringView(name.data(), name.size()));
    });
  }

  static void ResetContextGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      FATAL("Internal error: resetContextGroup(context_group_id).");
    }
    int context_group_id = args[0].As<v8::Int32>()->Value();
    RunSyncTask(backend_runner_, [&context_group_id](IsolateData* data) {
      data->ResetContextGroup(context_group_id);
    });
  }

  static void ConnectSession(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 3 || !args[0]->IsInt32() || !args[1]->IsString() ||
        !args[2]->IsFunction()) {
      FATAL(
          "Internal error: connectionSession(context_group_id, state, "
          "dispatch).");
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
      FATAL("Internal error: disconnectionSession(session_id).");
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
      FATAL("Internal error: sendMessageToBackend(session_id, message).");
    }
    backend_runner_->Append(std::make_unique<SendMessageToBackendTask>(
        args[0].As<v8::Int32>()->Value(),
        ToVector(args.GetIsolate(), args[1].As<v8::String>())));
  }

  static void InterruptForMessages(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    backend_runner_->InterruptForMessages();
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
    inspector->Set(isolate, "fireContextCreated",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::FireContextCreated));
    inspector->Set(isolate, "fireContextDestroyed",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::FireContextDestroyed));
    inspector->Set(
        isolate, "freeContext",
        v8::FunctionTemplate::New(isolate, &InspectorExtension::FreeContext));
    inspector->Set(isolate, "addInspectedObject",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::AddInspectedObject));
    inspector->Set(isolate, "setMaxAsyncTaskStacks",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::SetMaxAsyncTaskStacks));
    inspector->Set(
        isolate, "dumpAsyncTaskStacksStateForTest",
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::DumpAsyncTaskStacksStateForTest));
    inspector->Set(
        isolate, "breakProgram",
        v8::FunctionTemplate::New(isolate, &InspectorExtension::BreakProgram));
    inspector->Set(
        isolate, "createObjectWithStrictCheck",
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::CreateObjectWithStrictCheck));
    inspector->Set(isolate, "callWithScheduledBreak",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::CallWithScheduledBreak));
    inspector->Set(isolate, "allowAccessorFormatting",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::AllowAccessorFormatting));
    inspector->Set(
        isolate, "markObjectAsNotInspectable",
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::MarkObjectAsNotInspectable));
    inspector->Set(isolate, "createObjectWithAccessor",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::CreateObjectWithAccessor));
    inspector->Set(isolate, "storeCurrentStackTrace",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::StoreCurrentStackTrace));
    inspector->Set(isolate, "externalAsyncTaskStarted",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::ExternalAsyncTaskStarted));
    inspector->Set(
        isolate, "externalAsyncTaskFinished",
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::ExternalAsyncTaskFinished));
    inspector->Set(isolate, "scheduleWithAsyncStack",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::ScheduleWithAsyncStack));
    inspector->Set(
        isolate, "setAllowCodeGenerationFromStrings",
        v8::FunctionTemplate::New(
            isolate, &InspectorExtension::SetAllowCodeGenerationFromStrings));
    inspector->Set(isolate, "setResourceNamePrefix",
                   v8::FunctionTemplate::New(
                       isolate, &InspectorExtension::SetResourceNamePrefix));
    global->Set(isolate, "inspector", inspector);
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
      FATAL("Internal error: addInspectedObject(session_id, object).");
    }
    v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->AddInspectedObject(args[0].As<v8::Int32>()->Value(), args[1]);
  }

  static void SetMaxAsyncTaskStacks(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsInt32()) {
      FATAL("Internal error: setMaxAsyncTaskStacks(max).");
    }
    IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
        ->SetMaxAsyncTaskStacksForTest(args[0].As<v8::Int32>()->Value());
  }

  static void DumpAsyncTaskStacksStateForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 0) {
      FATAL("Internal error: dumpAsyncTaskStacksStateForTest().");
    }
    IsolateData::FromContext(args.GetIsolate()->GetCurrentContext())
        ->DumpAsyncTaskStacksStateForTest();
  }

  static void BreakProgram(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      FATAL("Internal error: breakProgram('reason', 'details').");
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
      FATAL("Internal error: createObjectWithStrictCheck().");
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
      FATAL("Internal error: callWithScheduledBreak('reason', 'details').");
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
      FATAL("Internal error: allowAccessorFormatting('object').");
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
      FATAL("Internal error: markObjectAsNotInspectable(object).");
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
      FATAL(
          "Internal error: createObjectWithAccessor('accessor name', "
          "hasSetter)\n");
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
      FATAL("Internal error: storeCurrentStackTrace('description')\n");
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
      FATAL("Internal error: externalAsyncTaskStarted(id)\n");
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
      FATAL("Internal error: externalAsyncTaskFinished(id)\n");
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
      FATAL(
          "Internal error: scheduleWithAsyncStack(function, 'task-name', "
          "with_empty_stack).");
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
      FATAL("Internal error: setAllowCodeGenerationFromStrings(allow).");
    }
    args.GetIsolate()->GetCurrentContext()->AllowCodeGenerationFromStrings(
        args[0].As<v8::Boolean>()->Value());
  }

  static void SetResourceNamePrefix(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1 || !args[0]->IsString()) {
      FATAL("Internal error: setResourceNamePrefix('prefix').");
    }
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    IsolateData* data = IsolateData::FromContext(context);
    data->SetResourceNamePrefix(v8::Local<v8::String>::Cast(args[0]));
  }
};

int InspectorTestMain(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  std::unique_ptr<Platform> platform(platform::NewDefaultPlatform());
  v8::V8::InitializePlatform(platform.get());
  FLAG_abort_on_contradictory_flags = true;
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::InitializeExternalStartupData(argv[0]);
  v8::V8::Initialize();
  i::DisableEmbeddedBlobRefcounting();

  base::Semaphore ready_semaphore(0);

  StartupData startup_data = {nullptr, 0};
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--embed") == 0) {
      argv[i++] = nullptr;
      printf("Embedding script '%s'\n", argv[i]);
      startup_data = i::CreateSnapshotDataBlobInternal(
          SnapshotCreator::FunctionCodeHandling::kClear, argv[i], nullptr);
      argv[i] = nullptr;
    }
  }

  {
    IsolateData::SetupGlobalTasks frontend_extensions;
    frontend_extensions.emplace_back(new UtilsExtension());
    TaskRunner frontend_runner(std::move(frontend_extensions),
                               kFailOnUncaughtExceptions, &ready_semaphore,
                               startup_data.data ? &startup_data : nullptr,
                               kNoInspector);
    ready_semaphore.Wait();

    int frontend_context_group_id = 0;
    RunSyncTask(&frontend_runner,
                [&frontend_context_group_id](IsolateData* data) {
                  frontend_context_group_id = data->CreateContextGroup();
                });

    IsolateData::SetupGlobalTasks backend_extensions;
    backend_extensions.emplace_back(new SetTimeoutExtension());
    backend_extensions.emplace_back(new InspectorExtension());
    TaskRunner backend_runner(
        std::move(backend_extensions), kStandardPropagateUncaughtExceptions,
        &ready_semaphore, startup_data.data ? &startup_data : nullptr,
        kWithInspector);
    ready_semaphore.Wait();
    UtilsExtension::set_backend_task_runner(&backend_runner);

    task_runners = {&frontend_runner, &backend_runner};

    for (int i = 1; i < argc; ++i) {
      // Ignore unknown flags.
      if (argv[i] == nullptr || argv[i][0] == '-') continue;

      bool exists = false;
      std::string chars = ReadFile(argv[i], &exists, true);
      if (!exists) {
        FATAL("Internal error: script file doesn't exists: %s\n", argv[i]);
      }
      frontend_runner.Append(std::make_unique<ExecuteStringTask>(
          chars, frontend_context_group_id));
    }

    frontend_runner.Join();
    backend_runner.Join();

    UtilsExtension::ClearAllSessions();
    delete[] startup_data.data;

    // TaskRunners go out of scope here, which causes Isolate teardown and all
    // running background tasks to be properly joined.
  }

  i::FreeCurrentEmbeddedBlob();
  return 0;
}
}  //  namespace

}  // namespace internal
}  // namespace v8

int main(int argc, char* argv[]) {
  return v8::internal::InspectorTestMain(argc, argv);
}
