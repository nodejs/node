// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_ISOLATE_DATA_H_
#define V8_TEST_INSPECTOR_PROTOCOL_ISOLATE_DATA_H_

#include <map>

#include "include/v8-inspector.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/vector.h"

class TaskRunner;

class IsolateData : public v8_inspector::V8InspectorClient {
 public:
  class SetupGlobalTask {
   public:
    virtual ~SetupGlobalTask() = default;
    virtual void Run(v8::Isolate* isolate,
                     v8::Local<v8::ObjectTemplate> global) = 0;
  };
  using SetupGlobalTasks = std::vector<std::unique_ptr<SetupGlobalTask>>;

  IsolateData(TaskRunner* task_runner, SetupGlobalTasks setup_global_tasks,
              v8::StartupData* startup_data, bool with_inspector);
  static IsolateData* FromContext(v8::Local<v8::Context> context);

  v8::Isolate* isolate() const { return isolate_.get(); }
  TaskRunner* task_runner() const { return task_runner_; }

  // Setting things up.
  int CreateContextGroup();
  v8::Local<v8::Context> GetContext(int context_group_id);
  int GetContextGroupId(v8::Local<v8::Context> context);
  void RegisterModule(v8::Local<v8::Context> context,
                      v8::internal::Vector<uint16_t> name,
                      v8::ScriptCompiler::Source* source);

  // Working with V8Inspector api.
  int ConnectSession(int context_group_id,
                     const v8_inspector::StringView& state,
                     v8_inspector::V8Inspector::Channel* channel);
  std::unique_ptr<v8_inspector::StringBuffer> DisconnectSession(int session_id);
  void SendMessage(int session_id, const v8_inspector::StringView& message);
  void BreakProgram(int context_group_id,
                    const v8_inspector::StringView& reason,
                    const v8_inspector::StringView& details);
  void SchedulePauseOnNextStatement(int context_group_id,
                                    const v8_inspector::StringView& reason,
                                    const v8_inspector::StringView& details);
  void CancelPauseOnNextStatement(int context_group_id);
  void AsyncTaskScheduled(const v8_inspector::StringView& name, void* task,
                          bool recurring);
  void AsyncTaskStarted(void* task);
  void AsyncTaskFinished(void* task);

  v8_inspector::V8StackTraceId StoreCurrentStackTrace(
      const v8_inspector::StringView& description);
  void ExternalAsyncTaskStarted(const v8_inspector::V8StackTraceId& parent);
  void ExternalAsyncTaskFinished(const v8_inspector::V8StackTraceId& parent);

  void AddInspectedObject(int session_id, v8::Local<v8::Value> object);

  // Test utilities.
  void SetCurrentTimeMS(double time);
  void SetMemoryInfo(v8::Local<v8::Value> memory_info);
  void SetLogConsoleApiMessageCalls(bool log);
  void SetLogMaxAsyncCallStackDepthChanged(bool log);
  void SetMaxAsyncTaskStacksForTest(int limit);
  void DumpAsyncTaskStacksStateForTest();
  void FireContextCreated(v8::Local<v8::Context> context, int context_group_id);
  void FireContextDestroyed(v8::Local<v8::Context> context);
  void FreeContext(v8::Local<v8::Context> context);
  void SetResourceNamePrefix(v8::Local<v8::String> prefix);

 private:
  struct VectorCompare {
    bool operator()(const v8::internal::Vector<uint16_t>& lhs,
                    const v8::internal::Vector<uint16_t>& rhs) const {
      for (int i = 0; i < lhs.length() && i < rhs.length(); ++i) {
        if (lhs[i] != rhs[i]) return lhs[i] < rhs[i];
      }
      return false;
    }
  };
  static v8::MaybeLocal<v8::Module> ModuleResolveCallback(
      v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);
  static void MessageHandler(v8::Local<v8::Message> message,
                             v8::Local<v8::Value> exception);
  static void PromiseRejectHandler(v8::PromiseRejectMessage data);
  static int HandleMessage(v8::Local<v8::Message> message,
                           v8::Local<v8::Value> exception);
  std::vector<int> GetSessionIds(int context_group_id);

  // V8InspectorClient implementation.
  bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
  v8::Local<v8::Context> ensureDefaultContextInGroup(
      int context_group_id) override;
  double currentTimeMS() override;
  v8::MaybeLocal<v8::Value> memoryInfo(v8::Isolate* isolate,
                                       v8::Local<v8::Context>) override;
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;
  void consoleAPIMessage(int contextGroupId,
                         v8::Isolate::MessageErrorLevel level,
                         const v8_inspector::StringView& message,
                         const v8_inspector::StringView& url,
                         unsigned lineNumber, unsigned columnNumber,
                         v8_inspector::V8StackTrace*) override;
  bool isInspectableHeapObject(v8::Local<v8::Object>) override;
  void maxAsyncCallStackDepthChanged(int depth) override;
  std::unique_ptr<v8_inspector::StringBuffer> resourceNameToUrl(
      const v8_inspector::StringView& resourceName) override;

  // The isolate gets deleted by its {Dispose} method, not by the default
  // deleter. Therefore we have to define a custom deleter for the unique_ptr to
  // call {Dispose}. We have to use the unique_ptr so that the isolate get
  // disposed in the right order, relative to other member variables.
  struct IsolateDeleter {
    void operator()(v8::Isolate* isolate) const { isolate->Dispose(); }
  };

  TaskRunner* task_runner_;
  SetupGlobalTasks setup_global_tasks_;
  std::unique_ptr<v8::Isolate, IsolateDeleter> isolate_;
  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  int last_context_group_id_ = 0;
  std::map<int, v8::Global<v8::Context>> contexts_;
  std::map<v8::internal::Vector<uint16_t>, v8::Global<v8::Module>,
           VectorCompare>
      modules_;
  int last_session_id_ = 0;
  std::map<int, std::unique_ptr<v8_inspector::V8InspectorSession>> sessions_;
  std::map<v8_inspector::V8InspectorSession*, int> context_group_by_session_;
  v8::Global<v8::Value> memory_info_;
  bool current_time_set_ = false;
  double current_time_ = 0.0;
  bool log_console_api_message_calls_ = false;
  bool log_max_async_call_stack_depth_changed_ = false;
  v8::Global<v8::Private> not_inspectable_private_;
  v8::Global<v8::String> resource_name_prefix_;

  DISALLOW_COPY_AND_ASSIGN(IsolateData);
};

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_ISOLATE_DATA_H_
