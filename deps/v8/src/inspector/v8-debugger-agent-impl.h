// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8DEBUGGERAGENTIMPL_H_
#define V8_INSPECTOR_V8DEBUGGERAGENTIMPL_H_

#include <vector>

#include "src/base/macros.h"
#include "src/debug/interface-types.h"
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

using protocol::Maybe;
using protocol::Response;

class V8DebuggerAgentImpl : public protocol::Debugger::Backend {
 public:
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
  Response enable() override;
  Response disable() override;
  Response setBreakpointsActive(bool active) override;
  Response setSkipAllPauses(bool skip) override;
  Response setBreakpointByUrl(
      int lineNumber, Maybe<String16> optionalURL,
      Maybe<String16> optionalURLRegex, Maybe<int> optionalColumnNumber,
      Maybe<String16> optionalCondition, String16*,
      std::unique_ptr<protocol::Array<protocol::Debugger::Location>>* locations)
      override;
  Response setBreakpoint(
      std::unique_ptr<protocol::Debugger::Location>,
      Maybe<String16> optionalCondition, String16*,
      std::unique_ptr<protocol::Debugger::Location>* actualLocation) override;
  Response removeBreakpoint(const String16& breakpointId) override;
  Response continueToLocation(std::unique_ptr<protocol::Debugger::Location>,
                              Maybe<String16> targetCallFrames) override;
  Response searchInContent(
      const String16& scriptId, const String16& query,
      Maybe<bool> optionalCaseSensitive, Maybe<bool> optionalIsRegex,
      std::unique_ptr<protocol::Array<protocol::Debugger::SearchMatch>>*)
      override;
  Response getPossibleBreakpoints(
      std::unique_ptr<protocol::Debugger::Location> start,
      Maybe<protocol::Debugger::Location> end, Maybe<bool> restrictToFunction,
      std::unique_ptr<protocol::Array<protocol::Debugger::BreakLocation>>*
          locations) override;
  Response setScriptSource(
      const String16& inScriptId, const String16& inScriptSource,
      Maybe<bool> dryRun,
      Maybe<protocol::Array<protocol::Debugger::CallFrame>>* optOutCallFrames,
      Maybe<bool>* optOutStackChanged,
      Maybe<protocol::Runtime::StackTrace>* optOutAsyncStackTrace,
      Maybe<protocol::Runtime::ExceptionDetails>* optOutCompileError) override;
  Response restartFrame(
      const String16& callFrameId,
      std::unique_ptr<protocol::Array<protocol::Debugger::CallFrame>>*
          newCallFrames,
      Maybe<protocol::Runtime::StackTrace>* asyncStackTrace) override;
  Response getScriptSource(const String16& scriptId,
                           String16* scriptSource) override;
  Response pause() override;
  Response resume() override;
  Response stepOver() override;
  Response stepInto() override;
  Response stepOut() override;
  void scheduleStepIntoAsync(
      std::unique_ptr<ScheduleStepIntoAsyncCallback> callback) override;
  Response setPauseOnExceptions(const String16& pauseState) override;
  Response evaluateOnCallFrame(
      const String16& callFrameId, const String16& expression,
      Maybe<String16> objectGroup, Maybe<bool> includeCommandLineAPI,
      Maybe<bool> silent, Maybe<bool> returnByValue,
      Maybe<bool> generatePreview, Maybe<bool> throwOnSideEffect,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result,
      Maybe<protocol::Runtime::ExceptionDetails>*) override;
  Response setVariableValue(
      int scopeNumber, const String16& variableName,
      std::unique_ptr<protocol::Runtime::CallArgument> newValue,
      const String16& callFrame) override;
  Response setAsyncCallStackDepth(int depth) override;
  Response setBlackboxPatterns(
      std::unique_ptr<protocol::Array<String16>> patterns) override;
  Response setBlackboxedRanges(
      const String16& scriptId,
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
  void didPause(int contextId, v8::Local<v8::Value> exception,
                const std::vector<String16>& hitBreakpoints,
                bool isPromiseRejection, bool isUncaught, bool isOOMBreak);
  void didContinue();
  void didParseSource(std::unique_ptr<V8DebuggerScript>, bool success);

  bool isFunctionBlackboxed(const String16& scriptId,
                            const v8::debug::Location& start,
                            const v8::debug::Location& end);

  bool skipAllPauses() const { return m_skipAllPauses; }

  v8::Isolate* isolate() { return m_isolate; }

 private:
  void enableImpl();

  Response currentCallFrames(
      std::unique_ptr<protocol::Array<protocol::Debugger::CallFrame>>*);
  std::unique_ptr<protocol::Runtime::StackTrace> currentAsyncStackTrace();

  void setPauseOnExceptionsImpl(int);

  std::unique_ptr<protocol::Debugger::Location> resolveBreakpoint(
      const String16& breakpointId, const ScriptBreakpoint&, BreakpointSource,
      const String16& hint);
  void removeBreakpointImpl(const String16& breakpointId);
  void clearBreakDetails();

  void internalSetAsyncCallStackDepth(int);
  void increaseCachedSkipStackGeneration();

  Response setBlackboxPattern(const String16& pattern);
  void resetBlackboxedStateCache();

  bool isPaused() const;

  using ScriptsMap =
      protocol::HashMap<String16, std::unique_ptr<V8DebuggerScript>>;
  using BreakpointIdToDebuggerBreakpointIdsMap =
      protocol::HashMap<String16, std::vector<String16>>;
  using DebugServerBreakpointToBreakpointIdAndSourceMap =
      protocol::HashMap<String16, std::pair<String16, BreakpointSource>>;
  using MuteBreakpoins = protocol::HashMap<String16, std::pair<String16, int>>;

  V8InspectorImpl* m_inspector;
  V8Debugger* m_debugger;
  V8InspectorSessionImpl* m_session;
  bool m_enabled;
  protocol::DictionaryValue* m_state;
  protocol::Debugger::Frontend m_frontend;
  v8::Isolate* m_isolate;
  JavaScriptCallFrames m_pausedCallFrames;
  ScriptsMap m_scripts;
  BreakpointIdToDebuggerBreakpointIdsMap m_breakpointIdToDebuggerBreakpointIds;
  DebugServerBreakpointToBreakpointIdAndSourceMap m_serverBreakpoints;

  using BreakReason =
      std::pair<String16, std::unique_ptr<protocol::DictionaryValue>>;
  std::vector<BreakReason> m_breakReason;

  void pushBreakDetails(
      const String16& breakReason,
      std::unique_ptr<protocol::DictionaryValue> breakAuxData);
  void popBreakDetails();

  bool m_skipAllPauses = false;

  std::unique_ptr<V8Regex> m_blackboxPattern;
  protocol::HashMap<String16, std::vector<std::pair<int, int>>>
      m_blackboxedPositions;

  DISALLOW_COPY_AND_ASSIGN(V8DebuggerAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8DEBUGGERAGENTIMPL_H_
