// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/diagnostics/etw-jit-win.h"

#include "include/v8-callbacks.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "src/api/api-inl.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/diagnostics/etw-jit-metadata-win.h"
#include "src/objects/shared-function-info.h"
#include "src/tasks/cancelable-task.h"
#include "src/tasks/task-utils.h"

#if !defined(V8_ENABLE_ETW_STACK_WALKING)
#error "This file is only compiled if v8_enable_etw_stack_walking"
#endif

namespace v8 {
namespace internal {
namespace ETWJITInterface {

V8_DECLARE_TRACELOGGING_PROVIDER(g_v8Provider);
V8_DEFINE_TRACELOGGING_PROVIDER(g_v8Provider);

std::atomic<bool> is_etw_enabled = false;

namespace {

class IsolateLoadScriptData {
 public:
  explicit IsolateLoadScriptData(Isolate* isolate) : isolate_(isolate) {}
  virtual ~IsolateLoadScriptData() {
    // When this is destroyed, it is because Isolate is being destroyed
    // also in Isolate::Deinit, that while already cancel all cancellable
    // tasks.
    if (HasPendingLog()) {
      pending_log_task_id_ = CancelableTaskManager::kInvalidTaskId;
    }
  }

  static void AddIsolate(Isolate* isolate);
  static void RemoveIsolate(Isolate* isolate);
  static void LogIsolatePendingLogs(Isolate* isolate);
  static void UpdateAllIsolates(bool etw_enabled);
  static bool MaybeAddLoadedScript(Isolate* isolate, int script_id);
  static void EnableLog(Isolate* isolate);
  static void DisableLog(Isolate* isolate);

 private:
  static IsolateLoadScriptData& GetData(Isolate* isolate);
  bool HasPendingLog() const {
    return pending_log_task_id_ != CancelableTaskManager::kInvalidTaskId;
  }
  void RemovePendingLog() {
    pending_log_task_id_ = CancelableTaskManager::kInvalidTaskId;
  }
  void CancelPendingLog() {
    DCHECK(HasPendingLog());
    isolate_->cancelable_task_manager()->TryAbort(pending_log_task_id_);
    RemovePendingLog();
  }
  void EnqueueEnableLog() {
    if (HasPendingLog()) {
      CancelPendingLog();
    }
    auto v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
    Isolate* isolate = isolate_;
    auto task = MakeCancelableTask(isolate_, [isolate] { EnableLog(isolate); });
    pending_log_task_id_ = task->id();
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(v8_isolate);
    taskrunner->PostTask(std::move(task));
  }
  void EnqueueDisableLog() {
    if (HasPendingLog()) {
      CancelPendingLog();
    }
    auto v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
    Isolate* isolate = isolate_;
    auto task =
        MakeCancelableTask(isolate_, [isolate] { DisableLog(isolate); });
    pending_log_task_id_ = task->id();
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(v8_isolate);
    taskrunner->PostTask(std::move(task));
  }

  bool IsScriptLoaded(int script_id) const {
    return loaded_scripts_ids_.find(script_id) != loaded_scripts_ids_.end();
  }
  void AddLoadedScript(int script_id) { loaded_scripts_ids_.insert(script_id); }
  void RemoveAllLoadedScripts() { loaded_scripts_ids_.clear(); }

  Isolate* isolate_ = nullptr;
  std::unordered_set<int> loaded_scripts_ids_;
  CancelableTaskManager::Id pending_log_task_id_ =
      CancelableTaskManager::kInvalidTaskId;
};

static base::LazyMutex isolates_mutex = LAZY_MUTEX_INITIALIZER;
using IsolateMapType =
    std::unordered_map<v8::internal::Isolate*, IsolateLoadScriptData>;
static base::LazyInstance<IsolateMapType>::type isolate_map =
    LAZY_INSTANCE_INITIALIZER;

IsolateLoadScriptData& IsolateLoadScriptData::GetData(Isolate* isolate) {
  return isolate_map.Pointer()->at(isolate);
}

void IsolateLoadScriptData::AddIsolate(Isolate* isolate) {
  base::MutexGuard guard(isolates_mutex.Pointer());
  isolate_map.Pointer()->emplace(isolate, IsolateLoadScriptData(isolate));
}

void IsolateLoadScriptData::RemoveIsolate(Isolate* isolate) {
  base::MutexGuard guard(isolates_mutex.Pointer());
  isolate_map.Pointer()->erase(isolate);
}

void IsolateLoadScriptData::EnableLog(Isolate* isolate) {
  bool has_pending_log = false;
  {
    base::MutexGuard guard(isolates_mutex.Pointer());
    auto& data = GetData(isolate);
    if (data.HasPendingLog()) {
      has_pending_log = true;
      data.CancelPendingLog();
    }
  }

  // This cannot be done while isolate_mutex is locked, as it can call
  // EventHandler while in the call for all the existing code.
  isolate->v8_file_logger()->SetEtwCodeEventHandler(
      has_pending_log ? kJitCodeEventEnumExisting : kJitCodeEventDefault);
}

void IsolateLoadScriptData::DisableLog(Isolate* isolate) {
  {
    base::MutexGuard guard(isolates_mutex.Pointer());
    auto& data = GetData(isolate);
    if (data.HasPendingLog()) {
      data.CancelPendingLog();
    }
    data.RemoveAllLoadedScripts();
  }
  isolate->v8_file_logger()->ResetEtwCodeEventHandler();
}

void IsolateLoadScriptData::UpdateAllIsolates(bool etw_enabled) {
  base::MutexGuard guard(isolates_mutex.Pointer());
  std::for_each(isolate_map.Pointer()->begin(), isolate_map.Pointer()->end(),
                [etw_enabled](auto& pair) {
                  auto& isolate_data = pair.second;
                  if (etw_enabled) {
                    isolate_data.EnqueueEnableLog();
                  } else {
                    isolate_data.EnqueueDisableLog();
                  }
                });
}

bool IsolateLoadScriptData::MaybeAddLoadedScript(Isolate* isolate,
                                                 int script_id) {
  base::MutexGuard guard(isolates_mutex.Pointer());
  auto& data = GetData(isolate);
  if (data.IsScriptLoaded(script_id)) {
    return false;
  }
  data.AddLoadedScript(script_id);
  return true;
}

}  // namespace

void EnableETWLog(Isolate* isolate) {
  IsolateLoadScriptData::EnableLog(isolate);
}

// TODO(v8/11911): UnboundScript::GetLineNumber should be replaced
SharedFunctionInfo GetSharedFunctionInfo(const JitCodeEvent* event) {
  return event->script.IsEmpty() ? SharedFunctionInfo()
                                 : *Utils::OpenHandle(*event->script);
}

int GetScriptLineNumber(const JitCodeEvent* event) {
  auto sfi = GetSharedFunctionInfo(event);
  return sfi.is_null()
             ? -1  // invalid sentinel number
             : Script::cast(sfi.script()).GetLineNumber(sfi.StartPosition()) +
                   1;
}

int GetScriptColumnNumber(const JitCodeEvent* event) {
  auto sfi = GetSharedFunctionInfo(event);
  return sfi.is_null()
             ? -1  // invalid sentinel number
             : Script::cast(sfi.script()).GetColumnNumber(sfi.StartPosition()) +
                   1;
}

std::wstring GetScriptMethodNameFromEvent(const JitCodeEvent* event) {
  int name_len = static_cast<int>(event->name.len);
  // Note: event->name.str is not null terminated.
  std::wstring method_name(name_len + 1, '\0');
  MultiByteToWideChar(
      CP_UTF8, 0, event->name.str, name_len,
      // Const cast needed as building with C++14 (not const in >= C++17)
      const_cast<LPWSTR>(method_name.data()),
      static_cast<int>(method_name.size()));
  return method_name;
}

std::wstring GetScriptMethodNameFromSharedFunctionInfo(
    const SharedFunctionInfo& sfi) {
  auto sfi_name = sfi.DebugNameCStr();
  int method_name_length = static_cast<int>(strlen(sfi_name.get()));
  std::wstring method_name(method_name_length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, sfi_name.get(), method_name_length,
                      const_cast<LPWSTR>(method_name.data()),
                      static_cast<int>(method_name.length()));
  return method_name;
}

std::wstring GetScriptMethodName(const JitCodeEvent* event) {
  auto sfi = GetSharedFunctionInfo(event);
  return sfi.is_null() ? GetScriptMethodNameFromEvent(event)
                       : GetScriptMethodNameFromSharedFunctionInfo(sfi);
}

void UpdateETWEnabled(bool enabled) {
  DCHECK(v8_flags.enable_etw_stack_walking);
  if (enabled == is_etw_enabled) {
    return;
  }
  is_etw_enabled = enabled;

  IsolateLoadScriptData::UpdateAllIsolates(enabled);
}

// This callback is invoked by Windows every time the ETW tracing status is
// changed for this application. As such, V8 needs to track its value for
// knowing if the event requires us to emit JIT runtime events.
void WINAPI ETWEnableCallback(LPCGUID /* source_id */, ULONG is_enabled,
                              UCHAR level, ULONGLONG match_any_keyword,
                              ULONGLONG match_all_keyword,
                              PEVENT_FILTER_DESCRIPTOR /* filter_data */,
                              PVOID /* callback_context */) {
  DCHECK(v8_flags.enable_etw_stack_walking);
  bool is_etw_enabled_now =
      is_enabled && level >= kTraceLevel &&
      (match_any_keyword & kJScriptRuntimeKeyword) &&
      ((match_all_keyword & kJScriptRuntimeKeyword) == match_all_keyword);

  UpdateETWEnabled(is_etw_enabled_now);
}

void Register() {
  DCHECK(!TraceLoggingProviderEnabled(g_v8Provider, 0, 0));
  TraceLoggingRegisterEx(g_v8Provider, ETWEnableCallback, nullptr);
}

void Unregister() {
  if (g_v8Provider) {
    TraceLoggingUnregister(g_v8Provider);
  }
  UpdateETWEnabled(false);
}

void AddIsolate(Isolate* isolate) {
  IsolateLoadScriptData::AddIsolate(isolate);
}

void RemoveIsolate(Isolate* isolate) {
  IsolateLoadScriptData::RemoveIsolate(isolate);
}

void EventHandler(const JitCodeEvent* event) {
  if (!is_etw_enabled) return;
  if (event->code_type != v8::JitCodeEvent::CodeType::JIT_CODE) return;
  if (event->type != v8::JitCodeEvent::EventType::CODE_ADDED) return;

  std::wstring method_name = GetScriptMethodName(event);

  v8::Isolate* script_context = event->isolate;
  v8::Local<v8::UnboundScript> script = event->script;
  int script_id = 0;
  if (!script.IsEmpty()) {
    // if the first time seeing this source file, log the SourceLoad event
    script_id = script->GetId();

    auto isolate = reinterpret_cast<Isolate*>(script_context);
    if (IsolateLoadScriptData::MaybeAddLoadedScript(isolate, script_id)) {
      v8::Local<v8::Value> script_name = script->GetScriptName();
      std::wstring wstr_name(0, L'\0');
      if (script_name->IsString()) {
        auto v8str_name = script_name.As<v8::String>();
        wstr_name.resize(v8str_name->Length());
        // On Windows wchar_t == uint16_t. const_Cast needed for C++14.
        uint16_t* wstr_data = const_cast<uint16_t*>(
            reinterpret_cast<const uint16_t*>(wstr_name.data()));
        v8str_name->Write(event->isolate, wstr_data);
      }

      constexpr static auto source_load_event_meta =
          EventMetadata(kSourceLoadEventID, kJScriptRuntimeKeyword);
      constexpr static auto source_load_event_fields = EventFields(
          "SourceLoad", Field("SourceID", TlgInUINT64),
          Field("ScriptContextID", TlgInPOINTER),
          Field("SourceFlags", TlgInUINT32), Field("Url", TlgInUNICODESTRING));
      LogEventData(g_v8Provider, &source_load_event_meta,
                   &source_load_event_fields, (uint64_t)script_id,
                   script_context,
                   (uint32_t)0,  // SourceFlags
                   wstr_name);
    }
  }

  constexpr static auto method_load_event_meta =
      EventMetadata(kMethodLoadEventID, kJScriptRuntimeKeyword);
  constexpr static auto method_load_event_fields = EventFields(
      "MethodLoad", Field("ScriptContextID", TlgInPOINTER),
      Field("MethodStartAddress", TlgInPOINTER),
      Field("MethodSize", TlgInUINT64), Field("MethodID", TlgInUINT32),
      Field("MethodFlags", TlgInUINT16),
      Field("MethodAddressRangeID", TlgInUINT16),
      Field("SourceID", TlgInUINT64), Field("Line", TlgInUINT32),
      Field("Column", TlgInUINT32), Field("MethodName", TlgInUNICODESTRING));
  LogEventData(g_v8Provider, &method_load_event_meta, &method_load_event_fields,
               script_context, event->code_start, (uint64_t)event->code_len,
               (uint32_t)0,  // MethodId
               (uint16_t)0,  // MethodFlags
               (uint16_t)0,  // MethodAddressRangeId
               (uint64_t)script_id, (uint32_t)GetScriptLineNumber(event),
               (uint32_t)GetScriptColumnNumber(event), method_name);
}

}  // namespace ETWJITInterface
}  // namespace internal
}  // namespace v8
