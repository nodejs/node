// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-agent-impl.h"

#include <algorithm>

#include "src/inspector/injected-script.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/java-script-call-frame.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/remote-object-id.h"
#include "src/inspector/script-breakpoint.h"
#include "src/inspector/search-util.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-script.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-regex.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

using protocol::Array;
using protocol::Maybe;
using protocol::Debugger::BreakpointId;
using protocol::Debugger::CallFrame;
using protocol::Runtime::ExceptionDetails;
using protocol::Runtime::ScriptId;
using protocol::Runtime::StackTrace;
using protocol::Runtime::RemoteObject;

namespace DebuggerAgentState {
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
static const char pauseOnExceptionsState[] = "pauseOnExceptionsState";
static const char asyncCallStackDepth[] = "asyncCallStackDepth";
static const char blackboxPattern[] = "blackboxPattern";
static const char debuggerEnabled[] = "debuggerEnabled";

// Breakpoint properties.
static const char url[] = "url";
static const char isRegex[] = "isRegex";
static const char lineNumber[] = "lineNumber";
static const char columnNumber[] = "columnNumber";
static const char condition[] = "condition";
static const char skipAllPauses[] = "skipAllPauses";

}  // namespace DebuggerAgentState

static const int maxSkipStepFrameCount = 128;
static const char backtraceObjectGroup[] = "backtrace";

static String16 breakpointIdSuffix(
    V8DebuggerAgentImpl::BreakpointSource source) {
  switch (source) {
    case V8DebuggerAgentImpl::UserBreakpointSource:
      break;
    case V8DebuggerAgentImpl::DebugCommandBreakpointSource:
      return ":debug";
    case V8DebuggerAgentImpl::MonitorCommandBreakpointSource:
      return ":monitor";
  }
  return String16();
}

static String16 generateBreakpointId(
    const String16& scriptId, int lineNumber, int columnNumber,
    V8DebuggerAgentImpl::BreakpointSource source) {
  return scriptId + ":" + String16::fromInteger(lineNumber) + ":" +
         String16::fromInteger(columnNumber) + breakpointIdSuffix(source);
}

static bool positionComparator(const std::pair<int, int>& a,
                               const std::pair<int, int>& b) {
  if (a.first != b.first) return a.first < b.first;
  return a.second < b.second;
}

static bool hasInternalError(ErrorString* errorString, bool hasError) {
  if (hasError) *errorString = "Internal error";
  return hasError;
}

static std::unique_ptr<protocol::Debugger::Location> buildProtocolLocation(
    const String16& scriptId, int lineNumber, int columnNumber) {
  return protocol::Debugger::Location::create()
      .setScriptId(scriptId)
      .setLineNumber(lineNumber)
      .setColumnNumber(columnNumber)
      .build();
}

V8DebuggerAgentImpl::V8DebuggerAgentImpl(
    V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel,
    protocol::DictionaryValue* state)
    : m_inspector(session->inspector()),
      m_debugger(m_inspector->debugger()),
      m_session(session),
      m_enabled(false),
      m_state(state),
      m_frontend(frontendChannel),
      m_isolate(m_inspector->isolate()),
      m_breakReason(protocol::Debugger::Paused::ReasonEnum::Other),
      m_scheduledDebuggerStep(NoStep),
      m_skipNextDebuggerStepOut(false),
      m_javaScriptPauseScheduled(false),
      m_steppingFromFramework(false),
      m_pausingOnNativeEvent(false),
      m_skippedStepFrameCount(0),
      m_recursionLevelForStepOut(0),
      m_recursionLevelForStepFrame(0),
      m_skipAllPauses(false) {
  clearBreakDetails();
}

V8DebuggerAgentImpl::~V8DebuggerAgentImpl() {}

bool V8DebuggerAgentImpl::checkEnabled(ErrorString* errorString) {
  if (enabled()) return true;
  *errorString = "Debugger agent is not enabled";
  return false;
}

void V8DebuggerAgentImpl::enable() {
  // m_inspector->addListener may result in reporting all parsed scripts to
  // the agent so it should already be in enabled state by then.
  m_enabled = true;
  m_state->setBoolean(DebuggerAgentState::debuggerEnabled, true);
  m_debugger->enable();

  std::vector<std::unique_ptr<V8DebuggerScript>> compiledScripts;
  m_debugger->getCompiledScripts(m_session->contextGroupId(), compiledScripts);
  for (size_t i = 0; i < compiledScripts.size(); i++)
    didParseSource(std::move(compiledScripts[i]), true);

  // FIXME(WK44513): breakpoints activated flag should be synchronized between
  // all front-ends
  m_debugger->setBreakpointsActivated(true);
}

bool V8DebuggerAgentImpl::enabled() { return m_enabled; }

void V8DebuggerAgentImpl::enable(ErrorString* errorString) {
  if (enabled()) return;

  if (!m_inspector->client()->canExecuteScripts(m_session->contextGroupId())) {
    *errorString = "Script execution is prohibited";
    return;
  }

  enable();
}

void V8DebuggerAgentImpl::disable(ErrorString*) {
  if (!enabled()) return;

  m_state->setObject(DebuggerAgentState::javaScriptBreakpoints,
                     protocol::DictionaryValue::create());
  m_state->setInteger(DebuggerAgentState::pauseOnExceptionsState,
                      V8Debugger::DontPauseOnExceptions);
  m_state->setInteger(DebuggerAgentState::asyncCallStackDepth, 0);

  if (!m_pausedContext.IsEmpty()) m_debugger->continueProgram();
  m_debugger->disable();
  m_pausedContext.Reset();
  JavaScriptCallFrames emptyCallFrames;
  m_pausedCallFrames.swap(emptyCallFrames);
  m_scripts.clear();
  m_blackboxedPositions.clear();
  m_breakpointIdToDebuggerBreakpointIds.clear();
  m_debugger->setAsyncCallStackDepth(this, 0);
  m_continueToLocationBreakpointId = String16();
  clearBreakDetails();
  m_scheduledDebuggerStep = NoStep;
  m_skipNextDebuggerStepOut = false;
  m_javaScriptPauseScheduled = false;
  m_steppingFromFramework = false;
  m_pausingOnNativeEvent = false;
  m_skippedStepFrameCount = 0;
  m_recursionLevelForStepFrame = 0;
  m_skipAllPauses = false;
  m_blackboxPattern = nullptr;
  m_state->remove(DebuggerAgentState::blackboxPattern);
  m_enabled = false;
  m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
}

void V8DebuggerAgentImpl::restore() {
  DCHECK(!m_enabled);
  if (!m_state->booleanProperty(DebuggerAgentState::debuggerEnabled, false))
    return;
  if (!m_inspector->client()->canExecuteScripts(m_session->contextGroupId()))
    return;

  enable();
  ErrorString error;

  int pauseState = V8Debugger::DontPauseOnExceptions;
  m_state->getInteger(DebuggerAgentState::pauseOnExceptionsState, &pauseState);
  setPauseOnExceptionsImpl(&error, pauseState);
  DCHECK(error.isEmpty());

  m_skipAllPauses =
      m_state->booleanProperty(DebuggerAgentState::skipAllPauses, false);

  int asyncCallStackDepth = 0;
  m_state->getInteger(DebuggerAgentState::asyncCallStackDepth,
                      &asyncCallStackDepth);
  m_debugger->setAsyncCallStackDepth(this, asyncCallStackDepth);

  String16 blackboxPattern;
  if (m_state->getString(DebuggerAgentState::blackboxPattern,
                         &blackboxPattern)) {
    if (!setBlackboxPattern(&error, blackboxPattern)) UNREACHABLE();
  }
}

void V8DebuggerAgentImpl::setBreakpointsActive(ErrorString* errorString,
                                               bool active) {
  if (!checkEnabled(errorString)) return;
  m_debugger->setBreakpointsActivated(active);
}

void V8DebuggerAgentImpl::setSkipAllPauses(ErrorString*, bool skip) {
  m_skipAllPauses = skip;
  m_state->setBoolean(DebuggerAgentState::skipAllPauses, m_skipAllPauses);
}

static std::unique_ptr<protocol::DictionaryValue>
buildObjectForBreakpointCookie(const String16& url, int lineNumber,
                               int columnNumber, const String16& condition,
                               bool isRegex) {
  std::unique_ptr<protocol::DictionaryValue> breakpointObject =
      protocol::DictionaryValue::create();
  breakpointObject->setString(DebuggerAgentState::url, url);
  breakpointObject->setInteger(DebuggerAgentState::lineNumber, lineNumber);
  breakpointObject->setInteger(DebuggerAgentState::columnNumber, columnNumber);
  breakpointObject->setString(DebuggerAgentState::condition, condition);
  breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
  return breakpointObject;
}

static bool matches(V8InspectorImpl* inspector, const String16& url,
                    const String16& pattern, bool isRegex) {
  if (isRegex) {
    V8Regex regex(inspector, pattern, true);
    return regex.match(url) != -1;
  }
  return url == pattern;
}

void V8DebuggerAgentImpl::setBreakpointByUrl(
    ErrorString* errorString, int lineNumber,
    const Maybe<String16>& optionalURL, const Maybe<String16>& optionalURLRegex,
    const Maybe<int>& optionalColumnNumber,
    const Maybe<String16>& optionalCondition, String16* outBreakpointId,
    std::unique_ptr<protocol::Array<protocol::Debugger::Location>>* locations) {
  *locations = Array<protocol::Debugger::Location>::create();
  if (optionalURL.isJust() == optionalURLRegex.isJust()) {
    *errorString = "Either url or urlRegex must be specified.";
    return;
  }

  String16 url = optionalURL.isJust() ? optionalURL.fromJust()
                                      : optionalURLRegex.fromJust();
  int columnNumber = 0;
  if (optionalColumnNumber.isJust()) {
    columnNumber = optionalColumnNumber.fromJust();
    if (columnNumber < 0) {
      *errorString = "Incorrect column number";
      return;
    }
  }
  String16 condition = optionalCondition.fromMaybe("");
  bool isRegex = optionalURLRegex.isJust();

  String16 breakpointId = (isRegex ? "/" + url + "/" : url) + ":" +
                          String16::fromInteger(lineNumber) + ":" +
                          String16::fromInteger(columnNumber);
  protocol::DictionaryValue* breakpointsCookie =
      m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
  if (!breakpointsCookie) {
    std::unique_ptr<protocol::DictionaryValue> newValue =
        protocol::DictionaryValue::create();
    breakpointsCookie = newValue.get();
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints,
                       std::move(newValue));
  }
  if (breakpointsCookie->get(breakpointId)) {
    *errorString = "Breakpoint at specified location already exists.";
    return;
  }

  breakpointsCookie->setObject(
      breakpointId, buildObjectForBreakpointCookie(
                        url, lineNumber, columnNumber, condition, isRegex));

  ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
  for (const auto& script : m_scripts) {
    if (!matches(m_inspector, script.second->sourceURL(), url, isRegex))
      continue;
    std::unique_ptr<protocol::Debugger::Location> location = resolveBreakpoint(
        breakpointId, script.first, breakpoint, UserBreakpointSource);
    if (location) (*locations)->addItem(std::move(location));
  }

  *outBreakpointId = breakpointId;
}

static bool parseLocation(
    ErrorString* errorString,
    std::unique_ptr<protocol::Debugger::Location> location, String16* scriptId,
    int* lineNumber, int* columnNumber) {
  *scriptId = location->getScriptId();
  *lineNumber = location->getLineNumber();
  *columnNumber = location->getColumnNumber(0);
  return true;
}

void V8DebuggerAgentImpl::setBreakpoint(
    ErrorString* errorString,
    std::unique_ptr<protocol::Debugger::Location> location,
    const Maybe<String16>& optionalCondition, String16* outBreakpointId,
    std::unique_ptr<protocol::Debugger::Location>* actualLocation) {
  String16 scriptId;
  int lineNumber;
  int columnNumber;

  if (!parseLocation(errorString, std::move(location), &scriptId, &lineNumber,
                     &columnNumber))
    return;

  String16 condition = optionalCondition.fromMaybe("");

  String16 breakpointId = generateBreakpointId(
      scriptId, lineNumber, columnNumber, UserBreakpointSource);
  if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) !=
      m_breakpointIdToDebuggerBreakpointIds.end()) {
    *errorString = "Breakpoint at specified location already exists.";
    return;
  }
  ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
  *actualLocation = resolveBreakpoint(breakpointId, scriptId, breakpoint,
                                      UserBreakpointSource);
  if (*actualLocation)
    *outBreakpointId = breakpointId;
  else
    *errorString = "Could not resolve breakpoint";
}

void V8DebuggerAgentImpl::removeBreakpoint(ErrorString* errorString,
                                           const String16& breakpointId) {
  if (!checkEnabled(errorString)) return;
  protocol::DictionaryValue* breakpointsCookie =
      m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
  if (breakpointsCookie) breakpointsCookie->remove(breakpointId);
  removeBreakpoint(breakpointId);
}

void V8DebuggerAgentImpl::removeBreakpoint(const String16& breakpointId) {
  DCHECK(enabled());
  BreakpointIdToDebuggerBreakpointIdsMap::iterator
      debuggerBreakpointIdsIterator =
          m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
  if (debuggerBreakpointIdsIterator ==
      m_breakpointIdToDebuggerBreakpointIds.end())
    return;
  const std::vector<String16>& ids = debuggerBreakpointIdsIterator->second;
  for (size_t i = 0; i < ids.size(); ++i) {
    const String16& debuggerBreakpointId = ids[i];

    m_debugger->removeBreakpoint(debuggerBreakpointId);
    m_serverBreakpoints.erase(debuggerBreakpointId);
  }
  m_breakpointIdToDebuggerBreakpointIds.erase(breakpointId);
}

void V8DebuggerAgentImpl::continueToLocation(
    ErrorString* errorString,
    std::unique_ptr<protocol::Debugger::Location> location) {
  if (!checkEnabled(errorString)) return;
  if (!m_continueToLocationBreakpointId.isEmpty()) {
    m_debugger->removeBreakpoint(m_continueToLocationBreakpointId);
    m_continueToLocationBreakpointId = "";
  }

  String16 scriptId;
  int lineNumber;
  int columnNumber;

  if (!parseLocation(errorString, std::move(location), &scriptId, &lineNumber,
                     &columnNumber))
    return;

  ScriptBreakpoint breakpoint(lineNumber, columnNumber, "");
  m_continueToLocationBreakpointId = m_debugger->setBreakpoint(
      scriptId, breakpoint, &lineNumber, &columnNumber);
  resume(errorString);
}

bool V8DebuggerAgentImpl::isCurrentCallStackEmptyOrBlackboxed() {
  DCHECK(enabled());
  JavaScriptCallFrames callFrames = m_debugger->currentCallFrames();
  for (size_t index = 0; index < callFrames.size(); ++index) {
    if (!isCallFrameWithUnknownScriptOrBlackboxed(callFrames[index].get()))
      return false;
  }
  return true;
}

bool V8DebuggerAgentImpl::isTopPausedCallFrameBlackboxed() {
  DCHECK(enabled());
  JavaScriptCallFrame* frame =
      m_pausedCallFrames.size() ? m_pausedCallFrames[0].get() : nullptr;
  return isCallFrameWithUnknownScriptOrBlackboxed(frame);
}

bool V8DebuggerAgentImpl::isCallFrameWithUnknownScriptOrBlackboxed(
    JavaScriptCallFrame* frame) {
  if (!frame) return true;
  ScriptsMap::iterator it =
      m_scripts.find(String16::fromInteger(frame->sourceID()));
  if (it == m_scripts.end()) {
    // Unknown scripts are blackboxed.
    return true;
  }
  if (m_blackboxPattern) {
    const String16& scriptSourceURL = it->second->sourceURL();
    if (!scriptSourceURL.isEmpty() &&
        m_blackboxPattern->match(scriptSourceURL) != -1)
      return true;
  }
  auto itBlackboxedPositions =
      m_blackboxedPositions.find(String16::fromInteger(frame->sourceID()));
  if (itBlackboxedPositions == m_blackboxedPositions.end()) return false;

  const std::vector<std::pair<int, int>>& ranges =
      itBlackboxedPositions->second;
  auto itRange = std::lower_bound(
      ranges.begin(), ranges.end(),
      std::make_pair(frame->line(), frame->column()), positionComparator);
  // Ranges array contains positions in script where blackbox state is changed.
  // [(0,0) ... ranges[0]) isn't blackboxed, [ranges[0] ... ranges[1]) is
  // blackboxed...
  return std::distance(ranges.begin(), itRange) % 2;
}

V8DebuggerAgentImpl::SkipPauseRequest
V8DebuggerAgentImpl::shouldSkipExceptionPause(
    JavaScriptCallFrame* topCallFrame) {
  if (m_steppingFromFramework) return RequestNoSkip;
  if (isCallFrameWithUnknownScriptOrBlackboxed(topCallFrame))
    return RequestContinue;
  return RequestNoSkip;
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::shouldSkipStepPause(
    JavaScriptCallFrame* topCallFrame) {
  if (m_steppingFromFramework) return RequestNoSkip;

  if (m_skipNextDebuggerStepOut) {
    m_skipNextDebuggerStepOut = false;
    if (m_scheduledDebuggerStep == StepOut) return RequestStepOut;
  }

  if (!isCallFrameWithUnknownScriptOrBlackboxed(topCallFrame))
    return RequestNoSkip;

  if (m_skippedStepFrameCount >= maxSkipStepFrameCount) return RequestStepOut;

  if (!m_skippedStepFrameCount) m_recursionLevelForStepFrame = 1;

  ++m_skippedStepFrameCount;
  return RequestStepFrame;
}

std::unique_ptr<protocol::Debugger::Location>
V8DebuggerAgentImpl::resolveBreakpoint(const String16& breakpointId,
                                       const String16& scriptId,
                                       const ScriptBreakpoint& breakpoint,
                                       BreakpointSource source) {
  DCHECK(enabled());
  // FIXME: remove these checks once crbug.com/520702 is resolved.
  CHECK(!breakpointId.isEmpty());
  CHECK(!scriptId.isEmpty());
  ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
  if (scriptIterator == m_scripts.end()) return nullptr;
  if (breakpoint.lineNumber < scriptIterator->second->startLine() ||
      scriptIterator->second->endLine() < breakpoint.lineNumber)
    return nullptr;

  int actualLineNumber;
  int actualColumnNumber;
  String16 debuggerBreakpointId = m_debugger->setBreakpoint(
      scriptId, breakpoint, &actualLineNumber, &actualColumnNumber);
  if (debuggerBreakpointId.isEmpty()) return nullptr;

  m_serverBreakpoints[debuggerBreakpointId] =
      std::make_pair(breakpointId, source);
  CHECK(!breakpointId.isEmpty());

  m_breakpointIdToDebuggerBreakpointIds[breakpointId].push_back(
      debuggerBreakpointId);
  return buildProtocolLocation(scriptId, actualLineNumber, actualColumnNumber);
}

void V8DebuggerAgentImpl::searchInContent(
    ErrorString* error, const String16& scriptId, const String16& query,
    const Maybe<bool>& optionalCaseSensitive,
    const Maybe<bool>& optionalIsRegex,
    std::unique_ptr<Array<protocol::Debugger::SearchMatch>>* results) {
  v8::HandleScope handles(m_isolate);
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end()) {
    *error = String16("No script for id: " + scriptId);
    return;
  }

  std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> matches =
      searchInTextByLinesImpl(m_session,
                              toProtocolString(it->second->source(m_isolate)),
                              query, optionalCaseSensitive.fromMaybe(false),
                              optionalIsRegex.fromMaybe(false));
  *results = protocol::Array<protocol::Debugger::SearchMatch>::create();
  for (size_t i = 0; i < matches.size(); ++i)
    (*results)->addItem(std::move(matches[i]));
}

void V8DebuggerAgentImpl::setScriptSource(
    ErrorString* errorString, const String16& scriptId,
    const String16& newContent, const Maybe<bool>& dryRun,
    Maybe<protocol::Array<protocol::Debugger::CallFrame>>* newCallFrames,
    Maybe<bool>* stackChanged, Maybe<StackTrace>* asyncStackTrace,
    Maybe<protocol::Runtime::ExceptionDetails>* optOutCompileError) {
  if (!checkEnabled(errorString)) return;

  v8::HandleScope handles(m_isolate);
  v8::Local<v8::String> newSource = toV8String(m_isolate, newContent);
  if (!m_debugger->setScriptSource(scriptId, newSource, dryRun.fromMaybe(false),
                                   errorString, optOutCompileError,
                                   &m_pausedCallFrames, stackChanged))
    return;

  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it != m_scripts.end()) it->second->setSource(m_isolate, newSource);

  std::unique_ptr<Array<CallFrame>> callFrames = currentCallFrames(errorString);
  if (!callFrames) return;
  *newCallFrames = std::move(callFrames);
  *asyncStackTrace = currentAsyncStackTrace();
}

void V8DebuggerAgentImpl::restartFrame(
    ErrorString* errorString, const String16& callFrameId,
    std::unique_ptr<Array<CallFrame>>* newCallFrames,
    Maybe<StackTrace>* asyncStackTrace) {
  if (!assertPaused(errorString)) return;
  InjectedScript::CallFrameScope scope(
      errorString, m_inspector, m_session->contextGroupId(), callFrameId);
  if (!scope.initialize()) return;
  if (scope.frameOrdinal() >= m_pausedCallFrames.size()) {
    *errorString = "Could not find call frame with given id";
    return;
  }

  v8::Local<v8::Value> resultValue;
  v8::Local<v8::Boolean> result;
  if (!m_pausedCallFrames[scope.frameOrdinal()]->restart().ToLocal(
          &resultValue) ||
      scope.tryCatch().HasCaught() ||
      !resultValue->ToBoolean(scope.context()).ToLocal(&result) ||
      !result->Value()) {
    *errorString = "Internal error";
    return;
  }
  JavaScriptCallFrames frames = m_debugger->currentCallFrames();
  m_pausedCallFrames.swap(frames);

  *newCallFrames = currentCallFrames(errorString);
  if (!*newCallFrames) return;
  *asyncStackTrace = currentAsyncStackTrace();
}

void V8DebuggerAgentImpl::getScriptSource(ErrorString* error,
                                          const String16& scriptId,
                                          String16* scriptSource) {
  if (!checkEnabled(error)) return;
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end()) {
    *error = "No script for id: " + scriptId;
    return;
  }
  v8::HandleScope handles(m_isolate);
  *scriptSource = toProtocolString(it->second->source(m_isolate));
}

void V8DebuggerAgentImpl::schedulePauseOnNextStatement(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() || m_scheduledDebuggerStep == StepInto ||
      m_javaScriptPauseScheduled || m_debugger->isPaused() ||
      !m_debugger->breakpointsActivated())
    return;
  m_breakReason = breakReason;
  m_breakAuxData = std::move(data);
  m_pausingOnNativeEvent = true;
  m_skipNextDebuggerStepOut = false;
  m_debugger->setPauseOnNextStatement(true);
}

void V8DebuggerAgentImpl::schedulePauseOnNextStatementIfSteppingInto() {
  DCHECK(enabled());
  if (m_scheduledDebuggerStep != StepInto || m_javaScriptPauseScheduled ||
      m_debugger->isPaused())
    return;
  clearBreakDetails();
  m_pausingOnNativeEvent = false;
  m_skippedStepFrameCount = 0;
  m_recursionLevelForStepFrame = 0;
  m_debugger->setPauseOnNextStatement(true);
}

void V8DebuggerAgentImpl::cancelPauseOnNextStatement() {
  if (m_javaScriptPauseScheduled || m_debugger->isPaused()) return;
  clearBreakDetails();
  m_pausingOnNativeEvent = false;
  m_debugger->setPauseOnNextStatement(false);
}

void V8DebuggerAgentImpl::pause(ErrorString* errorString) {
  if (!checkEnabled(errorString)) return;
  if (m_javaScriptPauseScheduled || m_debugger->isPaused()) return;
  clearBreakDetails();
  m_javaScriptPauseScheduled = true;
  m_scheduledDebuggerStep = NoStep;
  m_skippedStepFrameCount = 0;
  m_steppingFromFramework = false;
  m_debugger->setPauseOnNextStatement(true);
}

void V8DebuggerAgentImpl::resume(ErrorString* errorString) {
  if (!assertPaused(errorString)) return;
  m_scheduledDebuggerStep = NoStep;
  m_steppingFromFramework = false;
  m_session->releaseObjectGroup(backtraceObjectGroup);
  m_debugger->continueProgram();
}

void V8DebuggerAgentImpl::stepOver(ErrorString* errorString) {
  if (!assertPaused(errorString)) return;
  // StepOver at function return point should fallback to StepInto.
  JavaScriptCallFrame* frame =
      !m_pausedCallFrames.empty() ? m_pausedCallFrames[0].get() : nullptr;
  if (frame && frame->isAtReturn()) {
    stepInto(errorString);
    return;
  }
  m_scheduledDebuggerStep = StepOver;
  m_steppingFromFramework = isTopPausedCallFrameBlackboxed();
  m_session->releaseObjectGroup(backtraceObjectGroup);
  m_debugger->stepOverStatement();
}

void V8DebuggerAgentImpl::stepInto(ErrorString* errorString) {
  if (!assertPaused(errorString)) return;
  m_scheduledDebuggerStep = StepInto;
  m_steppingFromFramework = isTopPausedCallFrameBlackboxed();
  m_session->releaseObjectGroup(backtraceObjectGroup);
  m_debugger->stepIntoStatement();
}

void V8DebuggerAgentImpl::stepOut(ErrorString* errorString) {
  if (!assertPaused(errorString)) return;
  m_scheduledDebuggerStep = StepOut;
  m_skipNextDebuggerStepOut = false;
  m_recursionLevelForStepOut = 1;
  m_steppingFromFramework = isTopPausedCallFrameBlackboxed();
  m_session->releaseObjectGroup(backtraceObjectGroup);
  m_debugger->stepOutOfFunction();
}

void V8DebuggerAgentImpl::setPauseOnExceptions(
    ErrorString* errorString, const String16& stringPauseState) {
  if (!checkEnabled(errorString)) return;
  V8Debugger::PauseOnExceptionsState pauseState;
  if (stringPauseState == "none") {
    pauseState = V8Debugger::DontPauseOnExceptions;
  } else if (stringPauseState == "all") {
    pauseState = V8Debugger::PauseOnAllExceptions;
  } else if (stringPauseState == "uncaught") {
    pauseState = V8Debugger::PauseOnUncaughtExceptions;
  } else {
    *errorString = "Unknown pause on exceptions mode: " + stringPauseState;
    return;
  }
  setPauseOnExceptionsImpl(errorString, pauseState);
}

void V8DebuggerAgentImpl::setPauseOnExceptionsImpl(ErrorString* errorString,
                                                   int pauseState) {
  m_debugger->setPauseOnExceptionsState(
      static_cast<V8Debugger::PauseOnExceptionsState>(pauseState));
  if (m_debugger->getPauseOnExceptionsState() != pauseState)
    *errorString = "Internal error. Could not change pause on exceptions state";
  else
    m_state->setInteger(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

void V8DebuggerAgentImpl::evaluateOnCallFrame(
    ErrorString* errorString, const String16& callFrameId,
    const String16& expression, const Maybe<String16>& objectGroup,
    const Maybe<bool>& includeCommandLineAPI, const Maybe<bool>& silent,
    const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview,
    std::unique_ptr<RemoteObject>* result,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  if (!assertPaused(errorString)) return;
  InjectedScript::CallFrameScope scope(
      errorString, m_inspector, m_session->contextGroupId(), callFrameId);
  if (!scope.initialize()) return;
  if (scope.frameOrdinal() >= m_pausedCallFrames.size()) {
    *errorString = "Could not find call frame with given id";
    return;
  }

  if (includeCommandLineAPI.fromMaybe(false) && !scope.installCommandLineAPI())
    return;
  if (silent.fromMaybe(false)) scope.ignoreExceptionsAndMuteConsole();

  v8::MaybeLocal<v8::Value> maybeResultValue =
      m_pausedCallFrames[scope.frameOrdinal()]->evaluate(
          toV8String(m_isolate, expression));

  // Re-initialize after running client's code, as it could have destroyed
  // context or session.
  if (!scope.initialize()) return;
  scope.injectedScript()->wrapEvaluateResult(
      errorString, maybeResultValue, scope.tryCatch(),
      objectGroup.fromMaybe(""), returnByValue.fromMaybe(false),
      generatePreview.fromMaybe(false), result, exceptionDetails);
}

void V8DebuggerAgentImpl::setVariableValue(
    ErrorString* errorString, int scopeNumber, const String16& variableName,
    std::unique_ptr<protocol::Runtime::CallArgument> newValueArgument,
    const String16& callFrameId) {
  if (!checkEnabled(errorString)) return;
  if (!assertPaused(errorString)) return;
  InjectedScript::CallFrameScope scope(
      errorString, m_inspector, m_session->contextGroupId(), callFrameId);
  if (!scope.initialize()) return;

  v8::Local<v8::Value> newValue;
  if (!scope.injectedScript()
           ->resolveCallArgument(errorString, newValueArgument.get())
           .ToLocal(&newValue))
    return;

  if (scope.frameOrdinal() >= m_pausedCallFrames.size()) {
    *errorString = "Could not find call frame with given id";
    return;
  }
  v8::MaybeLocal<v8::Value> result =
      m_pausedCallFrames[scope.frameOrdinal()]->setVariableValue(
          scopeNumber, toV8String(m_isolate, variableName), newValue);
  if (scope.tryCatch().HasCaught() || result.IsEmpty()) {
    *errorString = "Internal error";
    return;
  }
}

void V8DebuggerAgentImpl::setAsyncCallStackDepth(ErrorString* errorString,
                                                 int depth) {
  if (!checkEnabled(errorString)) return;
  m_state->setInteger(DebuggerAgentState::asyncCallStackDepth, depth);
  m_debugger->setAsyncCallStackDepth(this, depth);
}

void V8DebuggerAgentImpl::setBlackboxPatterns(
    ErrorString* errorString,
    std::unique_ptr<protocol::Array<String16>> patterns) {
  if (!patterns->length()) {
    m_blackboxPattern = nullptr;
    m_state->remove(DebuggerAgentState::blackboxPattern);
    return;
  }

  String16Builder patternBuilder;
  patternBuilder.append('(');
  for (size_t i = 0; i < patterns->length() - 1; ++i) {
    patternBuilder.append(patterns->get(i));
    patternBuilder.append("|");
  }
  patternBuilder.append(patterns->get(patterns->length() - 1));
  patternBuilder.append(')');
  String16 pattern = patternBuilder.toString();
  if (!setBlackboxPattern(errorString, pattern)) return;
  m_state->setString(DebuggerAgentState::blackboxPattern, pattern);
}

bool V8DebuggerAgentImpl::setBlackboxPattern(ErrorString* errorString,
                                             const String16& pattern) {
  std::unique_ptr<V8Regex> regex(new V8Regex(
      m_inspector, pattern, true /** caseSensitive */, false /** multiline */));
  if (!regex->isValid()) {
    *errorString = "Pattern parser error: " + regex->errorMessage();
    return false;
  }
  m_blackboxPattern = std::move(regex);
  return true;
}

void V8DebuggerAgentImpl::setBlackboxedRanges(
    ErrorString* error, const String16& scriptId,
    std::unique_ptr<protocol::Array<protocol::Debugger::ScriptPosition>>
        inPositions) {
  if (m_scripts.find(scriptId) == m_scripts.end()) {
    *error = "No script with passed id.";
    return;
  }

  if (!inPositions->length()) {
    m_blackboxedPositions.erase(scriptId);
    return;
  }

  std::vector<std::pair<int, int>> positions;
  positions.reserve(inPositions->length());
  for (size_t i = 0; i < inPositions->length(); ++i) {
    protocol::Debugger::ScriptPosition* position = inPositions->get(i);
    if (position->getLineNumber() < 0) {
      *error = "Position missing 'line' or 'line' < 0.";
      return;
    }
    if (position->getColumnNumber() < 0) {
      *error = "Position missing 'column' or 'column' < 0.";
      return;
    }
    positions.push_back(
        std::make_pair(position->getLineNumber(), position->getColumnNumber()));
  }

  for (size_t i = 1; i < positions.size(); ++i) {
    if (positions[i - 1].first < positions[i].first) continue;
    if (positions[i - 1].first == positions[i].first &&
        positions[i - 1].second < positions[i].second)
      continue;
    *error =
        "Input positions array is not sorted or contains duplicate values.";
    return;
  }

  m_blackboxedPositions[scriptId] = positions;
}

void V8DebuggerAgentImpl::willExecuteScript(int scriptId) {
  changeJavaScriptRecursionLevel(+1);
  // Fast return.
  if (m_scheduledDebuggerStep != StepInto) return;
  schedulePauseOnNextStatementIfSteppingInto();
}

void V8DebuggerAgentImpl::didExecuteScript() {
  changeJavaScriptRecursionLevel(-1);
}

void V8DebuggerAgentImpl::changeJavaScriptRecursionLevel(int step) {
  if (m_javaScriptPauseScheduled && !m_skipAllPauses &&
      !m_debugger->isPaused()) {
    // Do not ever loose user's pause request until we have actually paused.
    m_debugger->setPauseOnNextStatement(true);
  }
  if (m_scheduledDebuggerStep == StepOut) {
    m_recursionLevelForStepOut += step;
    if (!m_recursionLevelForStepOut) {
      // When StepOut crosses a task boundary (i.e. js -> c++) from where it was
      // requested,
      // switch stepping to step into a next JS task, as if we exited to a
      // blackboxed framework.
      m_scheduledDebuggerStep = StepInto;
      m_skipNextDebuggerStepOut = false;
    }
  }
  if (m_recursionLevelForStepFrame) {
    m_recursionLevelForStepFrame += step;
    if (!m_recursionLevelForStepFrame) {
      // We have walked through a blackboxed framework and got back to where we
      // started.
      // If there was no stepping scheduled, we should cancel the stepping
      // explicitly,
      // since there may be a scheduled StepFrame left.
      // Otherwise, if we were stepping in/over, the StepFrame will stop at the
      // right location,
      // whereas if we were stepping out, we should continue doing so after
      // debugger pauses
      // from the old StepFrame.
      m_skippedStepFrameCount = 0;
      if (m_scheduledDebuggerStep == NoStep)
        m_debugger->clearStepping();
      else if (m_scheduledDebuggerStep == StepOut)
        m_skipNextDebuggerStepOut = true;
    }
  }
}

std::unique_ptr<Array<CallFrame>> V8DebuggerAgentImpl::currentCallFrames(
    ErrorString* errorString) {
  if (m_pausedContext.IsEmpty() || !m_pausedCallFrames.size())
    return Array<CallFrame>::create();
  ErrorString ignored;
  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Context> debuggerContext =
      v8::Debug::GetDebugContext(m_isolate);
  v8::Context::Scope contextScope(debuggerContext);

  v8::Local<v8::Array> objects = v8::Array::New(m_isolate);

  for (size_t frameOrdinal = 0; frameOrdinal < m_pausedCallFrames.size();
       ++frameOrdinal) {
    const std::unique_ptr<JavaScriptCallFrame>& currentCallFrame =
        m_pausedCallFrames[frameOrdinal];

    v8::Local<v8::Object> details = currentCallFrame->details();
    if (hasInternalError(errorString, details.IsEmpty()))
      return Array<CallFrame>::create();

    int contextId = currentCallFrame->contextId();
    InjectedScript* injectedScript =
        contextId ? m_session->findInjectedScript(&ignored, contextId)
                  : nullptr;

    String16 callFrameId =
        RemoteCallFrameId::serialize(contextId, static_cast<int>(frameOrdinal));
    if (hasInternalError(
            errorString,
            !details
                 ->Set(debuggerContext,
                       toV8StringInternalized(m_isolate, "callFrameId"),
                       toV8String(m_isolate, callFrameId))
                 .FromMaybe(false)))
      return Array<CallFrame>::create();

    if (injectedScript) {
      v8::Local<v8::Value> scopeChain;
      if (hasInternalError(
              errorString,
              !details->Get(debuggerContext,
                            toV8StringInternalized(m_isolate, "scopeChain"))
                      .ToLocal(&scopeChain) ||
                  !scopeChain->IsArray()))
        return Array<CallFrame>::create();
      v8::Local<v8::Array> scopeChainArray = scopeChain.As<v8::Array>();
      if (!injectedScript->wrapPropertyInArray(
              errorString, scopeChainArray,
              toV8StringInternalized(m_isolate, "object"),
              backtraceObjectGroup))
        return Array<CallFrame>::create();
      if (!injectedScript->wrapObjectProperty(
              errorString, details, toV8StringInternalized(m_isolate, "this"),
              backtraceObjectGroup))
        return Array<CallFrame>::create();
      if (details
              ->Has(debuggerContext,
                    toV8StringInternalized(m_isolate, "returnValue"))
              .FromMaybe(false)) {
        if (!injectedScript->wrapObjectProperty(
                errorString, details,
                toV8StringInternalized(m_isolate, "returnValue"),
                backtraceObjectGroup))
          return Array<CallFrame>::create();
      }
    } else {
      if (hasInternalError(errorString, !details
                                             ->Set(debuggerContext,
                                                   toV8StringInternalized(
                                                       m_isolate, "scopeChain"),
                                                   v8::Array::New(m_isolate, 0))
                                             .FromMaybe(false)))
        return Array<CallFrame>::create();
      v8::Local<v8::Object> remoteObject = v8::Object::New(m_isolate);
      if (hasInternalError(
              errorString,
              !remoteObject
                   ->Set(debuggerContext,
                         toV8StringInternalized(m_isolate, "type"),
                         toV8StringInternalized(m_isolate, "undefined"))
                   .FromMaybe(false)))
        return Array<CallFrame>::create();
      if (hasInternalError(errorString,
                           !details
                                ->Set(debuggerContext,
                                      toV8StringInternalized(m_isolate, "this"),
                                      remoteObject)
                                .FromMaybe(false)))
        return Array<CallFrame>::create();
      if (hasInternalError(
              errorString,
              !details
                   ->Delete(debuggerContext,
                            toV8StringInternalized(m_isolate, "returnValue"))
                   .FromMaybe(false)))
        return Array<CallFrame>::create();
    }

    if (hasInternalError(
            errorString,
            !objects
                 ->Set(debuggerContext, static_cast<int>(frameOrdinal), details)
                 .FromMaybe(false)))
      return Array<CallFrame>::create();
  }

  std::unique_ptr<protocol::Value> protocolValue =
      toProtocolValue(errorString, debuggerContext, objects);
  if (!protocolValue) return Array<CallFrame>::create();
  protocol::ErrorSupport errorSupport;
  std::unique_ptr<Array<CallFrame>> callFrames =
      Array<CallFrame>::parse(protocolValue.get(), &errorSupport);
  if (hasInternalError(errorString, !callFrames))
    return Array<CallFrame>::create();
  return callFrames;
}

std::unique_ptr<StackTrace> V8DebuggerAgentImpl::currentAsyncStackTrace() {
  if (m_pausedContext.IsEmpty()) return nullptr;
  V8StackTraceImpl* stackTrace = m_debugger->currentAsyncCallChain();
  return stackTrace ? stackTrace->buildInspectorObjectForTail(m_debugger)
                    : nullptr;
}

void V8DebuggerAgentImpl::didParseSource(
    std::unique_ptr<V8DebuggerScript> script, bool success) {
  v8::HandleScope handles(m_isolate);
  String16 scriptSource = toProtocolString(script->source(m_isolate));
  if (!success) script->setSourceURL(findSourceURL(scriptSource, false));
  if (!success)
    script->setSourceMappingURL(findSourceMapURL(scriptSource, false));

  std::unique_ptr<protocol::DictionaryValue> executionContextAuxData;
  if (!script->executionContextAuxData().isEmpty())
    executionContextAuxData = protocol::DictionaryValue::cast(
        protocol::parseJSON(script->executionContextAuxData()));
  bool isLiveEdit = script->isLiveEdit();
  bool hasSourceURL = script->hasSourceURL();
  String16 scriptId = script->scriptId();
  String16 scriptURL = script->sourceURL();

  const Maybe<String16>& sourceMapURLParam = script->sourceMappingURL();
  const Maybe<protocol::DictionaryValue>& executionContextAuxDataParam(
      std::move(executionContextAuxData));
  const bool* isLiveEditParam = isLiveEdit ? &isLiveEdit : nullptr;
  const bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : nullptr;
  if (success)
    m_frontend.scriptParsed(
        scriptId, scriptURL, script->startLine(), script->startColumn(),
        script->endLine(), script->endColumn(), script->executionContextId(),
        script->hash(), executionContextAuxDataParam, isLiveEditParam,
        sourceMapURLParam, hasSourceURLParam);
  else
    m_frontend.scriptFailedToParse(
        scriptId, scriptURL, script->startLine(), script->startColumn(),
        script->endLine(), script->endColumn(), script->executionContextId(),
        script->hash(), executionContextAuxDataParam, sourceMapURLParam,
        hasSourceURLParam);

  m_scripts[scriptId] = std::move(script);

  if (scriptURL.isEmpty() || !success) return;

  protocol::DictionaryValue* breakpointsCookie =
      m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
  if (!breakpointsCookie) return;

  for (size_t i = 0; i < breakpointsCookie->size(); ++i) {
    auto cookie = breakpointsCookie->at(i);
    protocol::DictionaryValue* breakpointObject =
        protocol::DictionaryValue::cast(cookie.second);
    bool isRegex;
    breakpointObject->getBoolean(DebuggerAgentState::isRegex, &isRegex);
    String16 url;
    breakpointObject->getString(DebuggerAgentState::url, &url);
    if (!matches(m_inspector, scriptURL, url, isRegex)) continue;
    ScriptBreakpoint breakpoint;
    breakpointObject->getInteger(DebuggerAgentState::lineNumber,
                                 &breakpoint.lineNumber);
    breakpointObject->getInteger(DebuggerAgentState::columnNumber,
                                 &breakpoint.columnNumber);
    breakpointObject->getString(DebuggerAgentState::condition,
                                &breakpoint.condition);
    std::unique_ptr<protocol::Debugger::Location> location = resolveBreakpoint(
        cookie.first, scriptId, breakpoint, UserBreakpointSource);
    if (location)
      m_frontend.breakpointResolved(cookie.first, std::move(location));
  }
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::didPause(
    v8::Local<v8::Context> context, v8::Local<v8::Value> exception,
    const std::vector<String16>& hitBreakpoints, bool isPromiseRejection) {
  JavaScriptCallFrames callFrames = m_debugger->currentCallFrames(1);
  JavaScriptCallFrame* topCallFrame =
      !callFrames.empty() ? callFrames.begin()->get() : nullptr;

  V8DebuggerAgentImpl::SkipPauseRequest result;
  if (m_skipAllPauses)
    result = RequestContinue;
  else if (!hitBreakpoints.empty())
    result = RequestNoSkip;  // Don't skip explicit breakpoints even if set in
                             // frameworks.
  else if (!exception.IsEmpty())
    result = shouldSkipExceptionPause(topCallFrame);
  else if (m_scheduledDebuggerStep != NoStep || m_javaScriptPauseScheduled ||
           m_pausingOnNativeEvent)
    result = shouldSkipStepPause(topCallFrame);
  else
    result = RequestNoSkip;

  m_skipNextDebuggerStepOut = false;
  if (result != RequestNoSkip) return result;
  // Skip pauses inside V8 internal scripts and on syntax errors.
  if (!topCallFrame) return RequestContinue;

  DCHECK(m_pausedContext.IsEmpty());
  JavaScriptCallFrames frames = m_debugger->currentCallFrames();
  m_pausedCallFrames.swap(frames);
  m_pausedContext.Reset(m_isolate, context);
  v8::HandleScope handles(m_isolate);

  if (!exception.IsEmpty()) {
    ErrorString ignored;
    InjectedScript* injectedScript =
        m_session->findInjectedScript(&ignored, V8Debugger::contextId(context));
    if (injectedScript) {
      m_breakReason =
          isPromiseRejection
              ? protocol::Debugger::Paused::ReasonEnum::PromiseRejection
              : protocol::Debugger::Paused::ReasonEnum::Exception;
      ErrorString errorString;
      auto obj = injectedScript->wrapObject(&errorString, exception,
                                            backtraceObjectGroup);
      m_breakAuxData = obj ? obj->serialize() : nullptr;
      // m_breakAuxData might be null after this.
    }
  }

  std::unique_ptr<Array<String16>> hitBreakpointIds = Array<String16>::create();

  for (const auto& point : hitBreakpoints) {
    DebugServerBreakpointToBreakpointIdAndSourceMap::iterator
        breakpointIterator = m_serverBreakpoints.find(point);
    if (breakpointIterator != m_serverBreakpoints.end()) {
      const String16& localId = breakpointIterator->second.first;
      hitBreakpointIds->addItem(localId);

      BreakpointSource source = breakpointIterator->second.second;
      if (m_breakReason == protocol::Debugger::Paused::ReasonEnum::Other &&
          source == DebugCommandBreakpointSource)
        m_breakReason = protocol::Debugger::Paused::ReasonEnum::DebugCommand;
    }
  }

  ErrorString errorString;
  m_frontend.paused(currentCallFrames(&errorString), m_breakReason,
                    std::move(m_breakAuxData), std::move(hitBreakpointIds),
                    currentAsyncStackTrace());
  m_scheduledDebuggerStep = NoStep;
  m_javaScriptPauseScheduled = false;
  m_steppingFromFramework = false;
  m_pausingOnNativeEvent = false;
  m_skippedStepFrameCount = 0;
  m_recursionLevelForStepFrame = 0;

  if (!m_continueToLocationBreakpointId.isEmpty()) {
    m_debugger->removeBreakpoint(m_continueToLocationBreakpointId);
    m_continueToLocationBreakpointId = "";
  }
  return result;
}

void V8DebuggerAgentImpl::didContinue() {
  m_pausedContext.Reset();
  JavaScriptCallFrames emptyCallFrames;
  m_pausedCallFrames.swap(emptyCallFrames);
  clearBreakDetails();
  m_frontend.resumed();
}

void V8DebuggerAgentImpl::breakProgram(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() || m_skipAllPauses || !m_pausedContext.IsEmpty() ||
      isCurrentCallStackEmptyOrBlackboxed() ||
      !m_debugger->breakpointsActivated())
    return;
  m_breakReason = breakReason;
  m_breakAuxData = std::move(data);
  m_scheduledDebuggerStep = NoStep;
  m_steppingFromFramework = false;
  m_pausingOnNativeEvent = false;
  m_debugger->breakProgram();
}

void V8DebuggerAgentImpl::breakProgramOnException(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() ||
      m_debugger->getPauseOnExceptionsState() ==
          V8Debugger::DontPauseOnExceptions)
    return;
  breakProgram(breakReason, std::move(data));
}

bool V8DebuggerAgentImpl::assertPaused(ErrorString* errorString) {
  if (m_pausedContext.IsEmpty()) {
    *errorString = "Can only perform operation while paused.";
    return false;
  }
  return true;
}

void V8DebuggerAgentImpl::clearBreakDetails() {
  m_breakReason = protocol::Debugger::Paused::ReasonEnum::Other;
  m_breakAuxData = nullptr;
}

void V8DebuggerAgentImpl::setBreakpointAt(const String16& scriptId,
                                          int lineNumber, int columnNumber,
                                          BreakpointSource source,
                                          const String16& condition) {
  String16 breakpointId =
      generateBreakpointId(scriptId, lineNumber, columnNumber, source);
  ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
  resolveBreakpoint(breakpointId, scriptId, breakpoint, source);
}

void V8DebuggerAgentImpl::removeBreakpointAt(const String16& scriptId,
                                             int lineNumber, int columnNumber,
                                             BreakpointSource source) {
  removeBreakpoint(
      generateBreakpointId(scriptId, lineNumber, columnNumber, source));
}

void V8DebuggerAgentImpl::reset() {
  if (!enabled()) return;
  m_scheduledDebuggerStep = NoStep;
  m_scripts.clear();
  m_blackboxedPositions.clear();
  m_breakpointIdToDebuggerBreakpointIds.clear();
}

}  // namespace v8_inspector
