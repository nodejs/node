// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8DEBUGGERAGENTIMPL_H_
#define V8_INSPECTOR_V8DEBUGGERAGENTIMPL_H_

#include <vector>

#include "src/base/macros.h"
#include "src/inspector/java-script-call-frame.h"
#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {

struct ScriptBreakpoint;
class JavaScriptCallFrame;
class PromiseTracker;
class V8Debugger;
class V8DebuggerScript;
class V8InspectorImpl;
class V8InspectorSessionImpl;
class V8Regex;
class V8StackTraceImpl;

using protocol::ErrorString;
using protocol::Maybe;

class V8DebuggerAgentImpl : public protocol::Debugger::Backend {
 public:
  enum SkipPauseRequest {
    RequestNoSkip,
    RequestContinue,
    RequestStepInto,
    RequestStepOut,
    RequestStepFrame
  };

  enum BreakpointSource {
    UserBreakpointSource,
    DebugCommandBreakpointSource,
    MonitorCommandBreakpointSource
  };

  V8DebuggerAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                      protocol::DictionaryValue* state);
  ~V8DebuggerAgentImpl() override;
  void restore();

  // Part of the protocol.
  void enable(ErrorString*) override;
  void disable(ErrorString*) override;
  void setBreakpointsActive(ErrorString*, bool active) override;
  void setSkipAllPauses(ErrorString*, bool skip) override;
  void setBreakpointByUrl(
      ErrorString*, int lineNumber, const Maybe<String16>& optionalURL,
      const Maybe<String16>& optionalURLRegex,
      const Maybe<int>& optionalColumnNumber,
      const Maybe<String16>& optionalCondition, String16*,
      std::unique_ptr<protocol::Array<protocol::Debugger::Location>>* locations)
      override;
  void setBreakpoint(
      ErrorString*, std::unique_ptr<protocol::Debugger::Location>,
      const Maybe<String16>& optionalCondition, String16*,
      std::unique_ptr<protocol::Debugger::Location>* actualLocation) override;
  void removeBreakpoint(ErrorString*, const String16& breakpointId) override;
  void continueToLocation(
      ErrorString*, std::unique_ptr<protocol::Debugger::Location>) override;
  void searchInContent(
      ErrorString*, const String16& scriptId, const String16& query,
      const Maybe<bool>& optionalCaseSensitive,
      const Maybe<bool>& optionalIsRegex,
      std::unique_ptr<protocol::Array<protocol::Debugger::SearchMatch>>*)
      override;
  void setScriptSource(
      ErrorString*, const String16& inScriptId, const String16& inScriptSource,
      const Maybe<bool>& dryRun,
      Maybe<protocol::Array<protocol::Debugger::CallFrame>>* optOutCallFrames,
      Maybe<bool>* optOutStackChanged,
      Maybe<protocol::Runtime::StackTrace>* optOutAsyncStackTrace,
      Maybe<protocol::Runtime::ExceptionDetails>* optOutCompileError) override;
  void restartFrame(
      ErrorString*, const String16& callFrameId,
      std::unique_ptr<protocol::Array<protocol::Debugger::CallFrame>>*
          newCallFrames,
      Maybe<protocol::Runtime::StackTrace>* asyncStackTrace) override;
  void getScriptSource(ErrorString*, const String16& scriptId,
                       String16* scriptSource) override;
  void pause(ErrorString*) override;
  void resume(ErrorString*) override;
  void stepOver(ErrorString*) override;
  void stepInto(ErrorString*) override;
  void stepOut(ErrorString*) override;
  void setPauseOnExceptions(ErrorString*, const String16& pauseState) override;
  void evaluateOnCallFrame(
      ErrorString*, const String16& callFrameId, const String16& expression,
      const Maybe<String16>& objectGroup,
      const Maybe<bool>& includeCommandLineAPI, const Maybe<bool>& silent,
      const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result,
      Maybe<protocol::Runtime::ExceptionDetails>*) override;
  void setVariableValue(
      ErrorString*, int scopeNumber, const String16& variableName,
      std::unique_ptr<protocol::Runtime::CallArgument> newValue,
      const String16& callFrame) override;
  void setAsyncCallStackDepth(ErrorString*, int depth) override;
  void setBlackboxPatterns(
      ErrorString*,
      std::unique_ptr<protocol::Array<String16>> patterns) override;
  void setBlackboxedRanges(
      ErrorString*, const String16& scriptId,
      std::unique_ptr<protocol::Array<protocol::Debugger::ScriptPosition>>
          positions) override;

  bool enabled();

  void setBreakpointAt(const String16& scriptId, int lineNumber,
                       int columnNumber, BreakpointSource,
                       const String16& condition = String16());
  void removeBreakpointAt(const String16& scriptId, int lineNumber,
                          int columnNumber, BreakpointSource);
  void schedulePauseOnNextStatement(
      const String16& breakReason,
      std::unique_ptr<protocol::DictionaryValue> data);
  void cancelPauseOnNextStatement();
  void breakProgram(const String16& breakReason,
                    std::unique_ptr<protocol::DictionaryValue> data);
  void breakProgramOnException(const String16& breakReason,
                               std::unique_ptr<protocol::DictionaryValue> data);

  void reset();

  // Interface for V8InspectorImpl
  SkipPauseRequest didPause(v8::Local<v8::Context>,
                            v8::Local<v8::Value> exception,
                            const std::vector<String16>& hitBreakpoints,
                            bool isPromiseRejection);
  void didContinue();
  void didParseSource(std::unique_ptr<V8DebuggerScript>, bool success);
  void willExecuteScript(int scriptId);
  void didExecuteScript();

  v8::Isolate* isolate() { return m_isolate; }

 private:
  bool checkEnabled(ErrorString*);
  void enable();

  SkipPauseRequest shouldSkipExceptionPause(JavaScriptCallFrame* topCallFrame);
  SkipPauseRequest shouldSkipStepPause(JavaScriptCallFrame* topCallFrame);

  void schedulePauseOnNextStatementIfSteppingInto();

  std::unique_ptr<protocol::Array<protocol::Debugger::CallFrame>>
  currentCallFrames(ErrorString*);
  std::unique_ptr<protocol::Runtime::StackTrace> currentAsyncStackTrace();

  void changeJavaScriptRecursionLevel(int step);

  void setPauseOnExceptionsImpl(ErrorString*, int);

  std::unique_ptr<protocol::Debugger::Location> resolveBreakpoint(
      const String16& breakpointId, const String16& scriptId,
      const ScriptBreakpoint&, BreakpointSource);
  void removeBreakpoint(const String16& breakpointId);
  bool assertPaused(ErrorString*);
  void clearBreakDetails();

  bool isCurrentCallStackEmptyOrBlackboxed();
  bool isTopPausedCallFrameBlackboxed();
  bool isCallFrameWithUnknownScriptOrBlackboxed(JavaScriptCallFrame*);

  void internalSetAsyncCallStackDepth(int);
  void increaseCachedSkipStackGeneration();

  bool setBlackboxPattern(ErrorString*, const String16& pattern);

  using ScriptsMap =
      protocol::HashMap<String16, std::unique_ptr<V8DebuggerScript>>;
  using BreakpointIdToDebuggerBreakpointIdsMap =
      protocol::HashMap<String16, std::vector<String16>>;
  using DebugServerBreakpointToBreakpointIdAndSourceMap =
      protocol::HashMap<String16, std::pair<String16, BreakpointSource>>;
  using MuteBreakpoins = protocol::HashMap<String16, std::pair<String16, int>>;

  enum DebuggerStep { NoStep = 0, StepInto, StepOver, StepOut };

  V8InspectorImpl* m_inspector;
  V8Debugger* m_debugger;
  V8InspectorSessionImpl* m_session;
  bool m_enabled;
  protocol::DictionaryValue* m_state;
  protocol::Debugger::Frontend m_frontend;
  v8::Isolate* m_isolate;
  v8::Global<v8::Context> m_pausedContext;
  JavaScriptCallFrames m_pausedCallFrames;
  ScriptsMap m_scripts;
  BreakpointIdToDebuggerBreakpointIdsMap m_breakpointIdToDebuggerBreakpointIds;
  DebugServerBreakpointToBreakpointIdAndSourceMap m_serverBreakpoints;
  String16 m_continueToLocationBreakpointId;
  String16 m_breakReason;
  std::unique_ptr<protocol::DictionaryValue> m_breakAuxData;
  DebuggerStep m_scheduledDebuggerStep;
  bool m_skipNextDebuggerStepOut;
  bool m_javaScriptPauseScheduled;
  bool m_steppingFromFramework;
  bool m_pausingOnNativeEvent;

  int m_skippedStepFrameCount;
  int m_recursionLevelForStepOut;
  int m_recursionLevelForStepFrame;
  bool m_skipAllPauses;

  std::unique_ptr<V8Regex> m_blackboxPattern;
  protocol::HashMap<String16, std::vector<std::pair<int, int>>>
      m_blackboxedPositions;

  DISALLOW_COPY_AND_ASSIGN(V8DebuggerAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8DEBUGGERAGENTIMPL_H_
