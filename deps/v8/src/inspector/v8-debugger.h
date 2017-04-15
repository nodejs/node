// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8DEBUGGER_H_
#define V8_INSPECTOR_V8DEBUGGER_H_

#include <vector>

#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/java-script-call-frame.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-debugger-script.h"
#include "src/inspector/wasm-translation.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

struct ScriptBreakpoint;
class V8DebuggerAgentImpl;
class V8InspectorImpl;
class V8StackTraceImpl;

using protocol::Response;

class V8Debugger {
 public:
  V8Debugger(v8::Isolate*, V8InspectorImpl*);
  ~V8Debugger();

  bool enabled() const;

  String16 setBreakpoint(const ScriptBreakpoint&, int* actualLineNumber,
                         int* actualColumnNumber);
  void removeBreakpoint(const String16& breakpointId);
  void setBreakpointsActivated(bool);
  bool breakpointsActivated() const { return m_breakpointsActivated; }

  v8::debug::ExceptionBreakState getPauseOnExceptionsState();
  void setPauseOnExceptionsState(v8::debug::ExceptionBreakState);
  void setPauseOnNextStatement(bool);
  bool canBreakProgram();
  void breakProgram();
  void continueProgram();
  void stepIntoStatement();
  void stepOverStatement();
  void stepOutOfFunction();
  void clearStepping();

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

  bool isPaused();
  v8::Local<v8::Context> pausedContext() { return m_pausedContext; }

  int maxAsyncCallChainDepth() { return m_maxAsyncCallStackDepth; }
  V8StackTraceImpl* currentAsyncCallChain();
  void setAsyncCallStackDepth(V8DebuggerAgentImpl*, int);
  std::unique_ptr<V8StackTraceImpl> createStackTrace(v8::Local<v8::StackTrace>);
  std::unique_ptr<V8StackTraceImpl> captureStackTrace(bool fullStack);

  v8::MaybeLocal<v8::Array> internalProperties(v8::Local<v8::Context>,
                                               v8::Local<v8::Value>);

  void asyncTaskScheduled(const StringView& taskName, void* task,
                          bool recurring);
  void asyncTaskScheduled(const String16& taskName, void* task, bool recurring);
  void asyncTaskCanceled(void* task);
  void asyncTaskStarted(void* task);
  void asyncTaskFinished(void* task);
  void allAsyncTasksCanceled();

  void muteScriptParsedEvents();
  void unmuteScriptParsedEvents();

  V8InspectorImpl* inspector() { return m_inspector; }

  WasmTranslation* wasmTranslation() { return &m_wasmTranslation; }

  void setMaxAsyncTaskStacksForTest(int limit) { m_maxAsyncCallStacks = limit; }

 private:
  void compileDebuggerScript();
  v8::MaybeLocal<v8::Value> callDebuggerMethod(const char* functionName,
                                               int argc,
                                               v8::Local<v8::Value> argv[],
                                               bool catchExceptions);
  v8::Local<v8::Context> debuggerContext() const;
  void clearBreakpoints();

  static void breakProgramCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  void handleProgramBreak(v8::Local<v8::Context> pausedContext,
                          v8::Local<v8::Object> executionState,
                          v8::Local<v8::Value> exception,
                          v8::Local<v8::Array> hitBreakpoints,
                          bool isPromiseRejection = false,
                          bool isUncaught = false);
  static void v8DebugEventCallback(const v8::debug::EventDetails&);
  v8::Local<v8::Value> callInternalGetterFunction(v8::Local<v8::Object>,
                                                  const char* functionName);
  void handleV8DebugEvent(const v8::debug::EventDetails&);
  static void v8AsyncTaskListener(v8::debug::PromiseDebugActionType type,
                                  int id, void* data);

  v8::Local<v8::Value> collectionEntries(v8::Local<v8::Context>,
                                         v8::Local<v8::Object>);
  v8::Local<v8::Value> generatorObjectLocation(v8::Local<v8::Context>,
                                               v8::Local<v8::Object>);
  v8::Local<v8::Value> functionLocation(v8::Local<v8::Context>,
                                        v8::Local<v8::Function>);

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

  v8::Isolate* m_isolate;
  V8InspectorImpl* m_inspector;
  int m_enableCount;
  bool m_breakpointsActivated;
  v8::Global<v8::Object> m_debuggerScript;
  v8::Global<v8::Context> m_debuggerContext;
  v8::Local<v8::Object> m_executionState;
  v8::Local<v8::Context> m_pausedContext;
  bool m_runningNestedMessageLoop;
  int m_ignoreScriptParsedEventsCounter;

  using AsyncTaskToStackTrace =
      protocol::HashMap<void*, std::unique_ptr<V8StackTraceImpl>>;
  AsyncTaskToStackTrace m_asyncTaskStacks;
  int m_maxAsyncCallStacks;
  std::map<int, void*> m_idToTask;
  std::unordered_map<void*, int> m_taskToId;
  int m_lastTaskId;
  protocol::HashSet<void*> m_recurringTasks;
  int m_maxAsyncCallStackDepth;
  std::vector<void*> m_currentTasks;
  std::vector<std::unique_ptr<V8StackTraceImpl>> m_currentStacks;
  protocol::HashMap<V8DebuggerAgentImpl*, int> m_maxAsyncCallStackDepthMap;

  v8::debug::ExceptionBreakState m_pauseOnExceptionsState;

  WasmTranslation m_wasmTranslation;

  DISALLOW_COPY_AND_ASSIGN(V8Debugger);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8DEBUGGER_H_
