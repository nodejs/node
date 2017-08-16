// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8DEBUGGER_H_
#define V8_INSPECTOR_V8DEBUGGER_H_

#include <list>
#include <vector>

#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/java-script-call-frame.h"
#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-debugger-script.h"
#include "src/inspector/wasm-translation.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

class AsyncStackTrace;
struct ScriptBreakpoint;
class StackFrame;
class V8Debugger;
class V8DebuggerAgentImpl;
class V8InspectorImpl;
class V8StackTraceImpl;

using protocol::Response;
using ScheduleStepIntoAsyncCallback =
    protocol::Debugger::Backend::ScheduleStepIntoAsyncCallback;

class V8Debugger : public v8::debug::DebugDelegate {
 public:
  V8Debugger(v8::Isolate*, V8InspectorImpl*);
  ~V8Debugger();

  bool enabled() const;
  v8::Isolate* isolate() const { return m_isolate; }

  String16 setBreakpoint(const ScriptBreakpoint&, int* actualLineNumber,
                         int* actualColumnNumber);
  void removeBreakpoint(const String16& breakpointId);
  void setBreakpointsActivated(bool);
  bool breakpointsActivated() const { return m_breakpointsActivated; }

  v8::debug::ExceptionBreakState getPauseOnExceptionsState();
  void setPauseOnExceptionsState(v8::debug::ExceptionBreakState);
  bool canBreakProgram();
  bool breakProgram(int targetContextGroupId);
  void continueProgram(int targetContextGroupId);

  void setPauseOnNextStatement(bool, int targetContextGroupId);
  void stepIntoStatement(int targetContextGroupId);
  void stepOverStatement(int targetContextGroupId);
  void stepOutOfFunction(int targetContextGroupId);
  void scheduleStepIntoAsync(
      std::unique_ptr<ScheduleStepIntoAsyncCallback> callback,
      int targetContextGroupId);

  Response continueToLocation(int targetContextGroupId,
                              std::unique_ptr<protocol::Debugger::Location>,
                              const String16& targetCallFramess);

  Response setScriptSource(
      const String16& sourceID, v8::Local<v8::String> newSource, bool dryRun,
      protocol::Maybe<protocol::Runtime::ExceptionDetails>*,
      JavaScriptCallFrames* newCallFrames, protocol::Maybe<bool>* stackChanged,
      bool* compileError);
  JavaScriptCallFrames currentCallFrames(int limit = 0);

  // Each script inherits debug data from v8::Context where it has been
  // compiled.
  // Only scripts whose debug data matches |contextGroupId| will be reported.
  // Passing 0 will result in reporting all scripts.
  void getCompiledScripts(int contextGroupId,
                          std::vector<std::unique_ptr<V8DebuggerScript>>&);
  void enable();
  void disable();

  bool isPaused() const { return m_pausedContextGroupId; }
  v8::Local<v8::Context> pausedContext() { return m_pausedContext; }

  int maxAsyncCallChainDepth() { return m_maxAsyncCallStackDepth; }
  void setAsyncCallStackDepth(V8DebuggerAgentImpl*, int);

  std::shared_ptr<AsyncStackTrace> currentAsyncParent();
  std::shared_ptr<AsyncStackTrace> currentAsyncCreation();

  std::shared_ptr<StackFrame> symbolize(v8::Local<v8::StackFrame> v8Frame);

  std::unique_ptr<V8StackTraceImpl> createStackTrace(v8::Local<v8::StackTrace>);
  std::unique_ptr<V8StackTraceImpl> captureStackTrace(bool fullStack);

  v8::MaybeLocal<v8::Array> internalProperties(v8::Local<v8::Context>,
                                               v8::Local<v8::Value>);

  void asyncTaskScheduled(const StringView& taskName, void* task,
                          bool recurring);
  void asyncTaskCanceled(void* task);
  void asyncTaskStarted(void* task);
  void asyncTaskFinished(void* task);
  void allAsyncTasksCanceled();

  void muteScriptParsedEvents();
  void unmuteScriptParsedEvents();

  V8InspectorImpl* inspector() { return m_inspector; }

  WasmTranslation* wasmTranslation() { return &m_wasmTranslation; }

  void setMaxAsyncTaskStacksForTest(int limit);
  void dumpAsyncTaskStacksStateForTest();

 private:
  void compileDebuggerScript();
  v8::MaybeLocal<v8::Value> callDebuggerMethod(const char* functionName,
                                               int argc,
                                               v8::Local<v8::Value> argv[],
                                               bool catchExceptions);
  v8::Local<v8::Context> debuggerContext() const;
  void clearBreakpoints();
  void clearContinueToLocation();
  bool shouldContinueToCurrentLocation();

  static void v8OOMCallback(void* data);

  void handleProgramBreak(v8::Local<v8::Context> pausedContext,
                          v8::Local<v8::Object> executionState,
                          v8::Local<v8::Value> exception,
                          v8::Local<v8::Array> hitBreakpoints,
                          bool isPromiseRejection = false,
                          bool isUncaught = false);

  enum ScopeTargetKind {
    FUNCTION,
    GENERATOR,
  };
  v8::MaybeLocal<v8::Value> getTargetScopes(v8::Local<v8::Context>,
                                            v8::Local<v8::Value>,
                                            ScopeTargetKind);

  v8::MaybeLocal<v8::Value> functionScopes(v8::Local<v8::Context>,
                                           v8::Local<v8::Function>);
  v8::MaybeLocal<v8::Value> generatorScopes(v8::Local<v8::Context>,
                                            v8::Local<v8::Value>);

  void asyncTaskCreatedForStack(void* task, void* parentTask);
  void asyncTaskScheduledForStack(const String16& taskName, void* task,
                                  bool recurring);
  void asyncTaskCanceledForStack(void* task);
  void asyncTaskStartedForStack(void* task);
  void asyncTaskFinishedForStack(void* task);

  void asyncTaskCandidateForStepping(void* task);
  void asyncTaskStartedForStepping(void* task);
  void asyncTaskFinishedForStepping(void* task);
  void asyncTaskCanceledForStepping(void* task);

  // v8::debug::DebugEventListener implementation.
  void PromiseEventOccurred(v8::debug::PromiseDebugActionType type, int id,
                            int parentId, bool createdByUser) override;
  void ScriptCompiled(v8::Local<v8::debug::Script> script,
                      bool has_compile_error) override;
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             v8::Local<v8::Object> exec_state,
                             v8::Local<v8::Value> break_points_hit) override;
  void ExceptionThrown(v8::Local<v8::Context> paused_context,
                       v8::Local<v8::Object> exec_state,
                       v8::Local<v8::Value> exception,
                       v8::Local<v8::Value> promise, bool is_uncaught) override;
  bool IsFunctionBlackboxed(v8::Local<v8::debug::Script> script,
                            const v8::debug::Location& start,
                            const v8::debug::Location& end) override;

  int currentContextGroupId();

  v8::Isolate* m_isolate;
  V8InspectorImpl* m_inspector;
  int m_enableCount;
  bool m_breakpointsActivated;
  v8::Global<v8::Object> m_debuggerScript;
  v8::Global<v8::Context> m_debuggerContext;
  v8::Local<v8::Object> m_executionState;
  v8::Local<v8::Context> m_pausedContext;
  int m_ignoreScriptParsedEventsCounter;
  bool m_scheduledOOMBreak = false;
  int m_targetContextGroupId = 0;
  int m_pausedContextGroupId = 0;
  String16 m_continueToLocationBreakpointId;
  String16 m_continueToLocationTargetCallFrames;
  std::unique_ptr<V8StackTraceImpl> m_continueToLocationStack;

  using AsyncTaskToStackTrace =
      protocol::HashMap<void*, std::weak_ptr<AsyncStackTrace>>;
  AsyncTaskToStackTrace m_asyncTaskStacks;
  AsyncTaskToStackTrace m_asyncTaskCreationStacks;
  protocol::HashSet<void*> m_recurringTasks;
  protocol::HashMap<void*, void*> m_parentTask;

  int m_maxAsyncCallStacks;
  int m_maxAsyncCallStackDepth;

  std::vector<void*> m_currentTasks;
  std::vector<std::shared_ptr<AsyncStackTrace>> m_currentAsyncParent;
  std::vector<std::shared_ptr<AsyncStackTrace>> m_currentAsyncCreation;

  void collectOldAsyncStacksIfNeeded();
  int m_asyncStacksCount = 0;
  // V8Debugger owns all the async stacks, while most of the other references
  // are weak, which allows to collect some stacks when there are too many.
  std::list<std::shared_ptr<AsyncStackTrace>> m_allAsyncStacks;
  std::map<int, std::weak_ptr<StackFrame>> m_framesCache;

  protocol::HashMap<V8DebuggerAgentImpl*, int> m_maxAsyncCallStackDepthMap;
  void* m_taskWithScheduledBreak = nullptr;

  std::unique_ptr<ScheduleStepIntoAsyncCallback> m_stepIntoAsyncCallback;
  bool m_breakRequested = false;

  v8::debug::ExceptionBreakState m_pauseOnExceptionsState;

  WasmTranslation m_wasmTranslation;

  DISALLOW_COPY_AND_ASSIGN(V8Debugger);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8DEBUGGER_H_
