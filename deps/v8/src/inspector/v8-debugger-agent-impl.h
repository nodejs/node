// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEBUGGER_AGENT_IMPL_H_
#define V8_INSPECTOR_V8_DEBUGGER_AGENT_IMPL_H_

#include <deque>
#include <unordered_map>
#include <vector>

#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/debug/interface-types.h"
#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {

struct ScriptBreakpoint;
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
  Response enable(Maybe<double> maxScriptsCacheSize,
                  String16* outDebuggerId) override;
  Response disable() override;
  Response setBreakpointsActive(bool active) override;
  Response setSkipAllPauses(bool skip) override;
  Response setBreakpointByUrl(
      int lineNumber, Maybe<String16> optionalURL,
      Maybe<String16> optionalURLRegex, Maybe<String16> optionalScriptHash,
      Maybe<int> optionalColumnNumber, Maybe<String16> optionalCondition,
      String16*,
      std::unique_ptr<protocol::Array<protocol::Debugger::Location>>* locations)
      override;
  Response setBreakpoint(
      std::unique_ptr<protocol::Debugger::Location>,
      Maybe<String16> optionalCondition, String16*,
      std::unique_ptr<protocol::Debugger::Location>* actualLocation) override;
  Response setBreakpointOnFunctionCall(const String16& functionObjectId,
                                       Maybe<String16> optionalCondition,
                                       String16* outBreakpointId) override;
  Response setInstrumentationBreakpoint(const String16& instrumentation,
                                        String16* outBreakpointId) override;
  Response removeBreakpoint(const String16& breakpointId) override;
  Response continueToLocation(std::unique_ptr<protocol::Debugger::Location>,
                              Maybe<String16> targetCallFrames) override;
  Response getStackTrace(
      std::unique_ptr<protocol::Runtime::StackTraceId> inStackTraceId,
      std::unique_ptr<protocol::Runtime::StackTrace>* outStackTrace) override;
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
      Maybe<protocol::Runtime::StackTraceId>* optOutAsyncStackTraceId,
      Maybe<protocol::Runtime::ExceptionDetails>* optOutCompileError) override;
  Response restartFrame(
      const String16& callFrameId,
      std::unique_ptr<protocol::Array<protocol::Debugger::CallFrame>>*
          newCallFrames,
      Maybe<protocol::Runtime::StackTrace>* asyncStackTrace,
      Maybe<protocol::Runtime::StackTraceId>* asyncStackTraceId) override;
  Response getScriptSource(const String16& scriptId,
                           String16* scriptSource) override;
  Response pause() override;
  Response resume() override;
  Response stepOver() override;
  Response stepInto(Maybe<bool> inBreakOnAsyncCall) override;
  Response stepOut() override;
  Response pauseOnAsyncCall(std::unique_ptr<protocol::Runtime::StackTraceId>
                                inParentStackTraceId) override;
  Response setPauseOnExceptions(const String16& pauseState) override;
  Response evaluateOnCallFrame(
      const String16& callFrameId, const String16& expression,
      Maybe<String16> objectGroup, Maybe<bool> includeCommandLineAPI,
      Maybe<bool> silent, Maybe<bool> returnByValue,
      Maybe<bool> generatePreview, Maybe<bool> throwOnSideEffect,
      Maybe<double> timeout,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result,
      Maybe<protocol::Runtime::ExceptionDetails>*) override;
  Response setVariableValue(
      int scopeNumber, const String16& variableName,
      std::unique_ptr<protocol::Runtime::CallArgument> newValue,
      const String16& callFrame) override;
  Response setReturnValue(
      std::unique_ptr<protocol::Runtime::CallArgument> newValue) override;
  Response setAsyncCallStackDepth(int depth) override;
  Response setBlackboxPatterns(
      std::unique_ptr<protocol::Array<String16>> patterns) override;
  Response setBlackboxedRanges(
      const String16& scriptId,
      std::unique_ptr<protocol::Array<protocol::Debugger::ScriptPosition>>
          positions) override;

  bool enabled() const { return m_enabled; }

  void setBreakpointFor(v8::Local<v8::Function> function,
                        v8::Local<v8::String> condition,
                        BreakpointSource source);
  void removeBreakpointFor(v8::Local<v8::Function> function,
                           BreakpointSource source);
  void schedulePauseOnNextStatement(
      const String16& breakReason,
      std::unique_ptr<protocol::DictionaryValue> data);
  void cancelPauseOnNextStatement();
  void breakProgram(const String16& breakReason,
                    std::unique_ptr<protocol::DictionaryValue> data);

  void reset();

  // Interface for V8InspectorImpl
  void didPause(int contextId, v8::Local<v8::Value> exception,
                const std::vector<v8::debug::BreakpointId>& hitBreakpoints,
                v8::debug::ExceptionType exceptionType, bool isUncaught,
                bool isOOMBreak, bool isAssert);
  void didContinue();
  void didParseSource(std::unique_ptr<V8DebuggerScript>, bool success);

  bool isFunctionBlackboxed(const String16& scriptId,
                            const v8::debug::Location& start,
                            const v8::debug::Location& end);

  bool acceptsPause(bool isOOMBreak) const;

  void ScriptCollected(const V8DebuggerScript* script);

  v8::Isolate* isolate() { return m_isolate; }

 private:
  void enableImpl();

  Response currentCallFrames(
      std::unique_ptr<protocol::Array<protocol::Debugger::CallFrame>>*);
  std::unique_ptr<protocol::Runtime::StackTrace> currentAsyncStackTrace();
  std::unique_ptr<protocol::Runtime::StackTraceId> currentExternalStackTrace();
  std::unique_ptr<protocol::Runtime::StackTraceId> currentScheduledAsyncCall();

  void setPauseOnExceptionsImpl(int);

  std::unique_ptr<protocol::Debugger::Location> setBreakpointImpl(
      const String16& breakpointId, const String16& scriptId,
      const String16& condition, int lineNumber, int columnNumber);
  void setBreakpointImpl(const String16& breakpointId,
                         v8::Local<v8::Function> function,
                         v8::Local<v8::String> condition);
  void removeBreakpointImpl(const String16& breakpointId);
  void clearBreakDetails();

  void internalSetAsyncCallStackDepth(int);
  void increaseCachedSkipStackGeneration();

  Response setBlackboxPattern(const String16& pattern);
  void resetBlackboxedStateCache();

  bool isPaused() const;

  void setScriptInstrumentationBreakpointIfNeeded(V8DebuggerScript* script);

  using ScriptsMap =
      std::unordered_map<String16, std::unique_ptr<V8DebuggerScript>>;
  using BreakpointIdToDebuggerBreakpointIdsMap =
      std::unordered_map<String16, std::vector<v8::debug::BreakpointId>>;
  using DebuggerBreakpointIdToBreakpointIdMap =
      std::unordered_map<v8::debug::BreakpointId, String16>;

  V8InspectorImpl* m_inspector;
  V8Debugger* m_debugger;
  V8InspectorSessionImpl* m_session;
  bool m_enabled;
  protocol::DictionaryValue* m_state;
  protocol::Debugger::Frontend m_frontend;
  v8::Isolate* m_isolate;
  ScriptsMap m_scripts;
  BreakpointIdToDebuggerBreakpointIdsMap m_breakpointIdToDebuggerBreakpointIds;
  DebuggerBreakpointIdToBreakpointIdMap m_debuggerBreakpointIdToBreakpointId;
  std::unordered_map<v8::debug::BreakpointId,
                     std::unique_ptr<protocol::DictionaryValue>>
      m_breakpointsOnScriptRun;

  size_t m_maxScriptCacheSize = 0;
  size_t m_cachedScriptSize = 0;
  std::deque<String16> m_cachedScriptIds;

  using BreakReason =
      std::pair<String16, std::unique_ptr<protocol::DictionaryValue>>;
  std::vector<BreakReason> m_breakReason;

  void pushBreakDetails(
      const String16& breakReason,
      std::unique_ptr<protocol::DictionaryValue> breakAuxData);
  void popBreakDetails();

  bool m_skipAllPauses = false;
  bool m_breakpointsActive = false;

  std::unique_ptr<V8Regex> m_blackboxPattern;
  std::unordered_map<String16, std::vector<std::pair<int, int>>>
      m_blackboxedPositions;

  DISALLOW_COPY_AND_ASSIGN(V8DebuggerAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEBUGGER_AGENT_IMPL_H_
