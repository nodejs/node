// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-agent-impl.h"

#include <algorithm>

#include "src/debug/debug-interface.h"
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
#include "src/inspector/v8-value-copier.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

using protocol::Array;
using protocol::Maybe;
using protocol::Debugger::BreakpointId;
using protocol::Debugger::CallFrame;
using protocol::Runtime::ExceptionDetails;
using protocol::Runtime::ScriptId;
using protocol::Runtime::RemoteObject;

namespace DebuggerAgentState {
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
static const char pauseOnExceptionsState[] = "pauseOnExceptionsState";
static const char asyncCallStackDepth[] = "asyncCallStackDepth";
static const char blackboxPattern[] = "blackboxPattern";
static const char debuggerEnabled[] = "debuggerEnabled";
static const char skipAllPauses[] = "skipAllPauses";

// Breakpoint properties.
static const char url[] = "url";
static const char isRegex[] = "isRegex";
static const char lineNumber[] = "lineNumber";
static const char columnNumber[] = "columnNumber";
static const char condition[] = "condition";
static const char hint[] = "hint";

}  // namespace DebuggerAgentState

static const char kBacktraceObjectGroup[] = "backtrace";
static const char kDebuggerNotEnabled[] = "Debugger agent is not enabled";
static const char kDebuggerNotPaused[] =
    "Can only perform operation while paused.";

static const size_t kBreakpointHintMaxLength = 128;
static const intptr_t kBreakpointHintMaxSearchOffset = 80 * 10;

namespace {

void TranslateWasmStackTraceLocations(Array<CallFrame>* stackTrace,
                                      WasmTranslation* wasmTranslation) {
  for (size_t i = 0, e = stackTrace->length(); i != e; ++i) {
    protocol::Debugger::Location* location = stackTrace->get(i)->getLocation();
    String16 scriptId = location->getScriptId();
    int lineNumber = location->getLineNumber();
    int columnNumber = location->getColumnNumber(-1);

    if (!wasmTranslation->TranslateWasmScriptLocationToProtocolLocation(
            &scriptId, &lineNumber, &columnNumber)) {
      continue;
    }

    location->setScriptId(std::move(scriptId));
    location->setLineNumber(lineNumber);
    location->setColumnNumber(columnNumber);
  }
}

String16 breakpointIdSuffix(V8DebuggerAgentImpl::BreakpointSource source) {
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

String16 generateBreakpointId(const ScriptBreakpoint& breakpoint,
                              V8DebuggerAgentImpl::BreakpointSource source) {
  String16Builder builder;
  builder.append(breakpoint.script_id);
  builder.append(':');
  builder.appendNumber(breakpoint.line_number);
  builder.append(':');
  builder.appendNumber(breakpoint.column_number);
  builder.append(breakpointIdSuffix(source));
  return builder.toString();
}

bool positionComparator(const std::pair<int, int>& a,
                        const std::pair<int, int>& b) {
  if (a.first != b.first) return a.first < b.first;
  return a.second < b.second;
}

std::unique_ptr<protocol::Debugger::Location> buildProtocolLocation(
    const String16& scriptId, int lineNumber, int columnNumber) {
  return protocol::Debugger::Location::create()
      .setScriptId(scriptId)
      .setLineNumber(lineNumber)
      .setColumnNumber(columnNumber)
      .build();
}

String16 breakpointHint(const V8DebuggerScript& script,
                        const ScriptBreakpoint& breakpoint) {
  int offset = script.offset(breakpoint.line_number, breakpoint.column_number);
  if (offset == V8DebuggerScript::kNoOffset) return String16();
  const String16& source = script.source();
  String16 hint =
      source.substring(offset, kBreakpointHintMaxLength).stripWhiteSpace();
  for (size_t i = 0; i < hint.length(); ++i) {
    if (hint[i] == '\r' || hint[i] == '\n' || hint[i] == ';') {
      return hint.substring(0, i);
    }
  }
  return hint;
}

void adjustBreakpointLocation(const V8DebuggerScript& script,
                              const String16& hint,
                              ScriptBreakpoint* breakpoint) {
  if (hint.isEmpty()) return;
  intptr_t sourceOffset =
      script.offset(breakpoint->line_number, breakpoint->column_number);
  if (sourceOffset == V8DebuggerScript::kNoOffset) return;

  intptr_t searchRegionOffset = std::max(
      sourceOffset - kBreakpointHintMaxSearchOffset, static_cast<intptr_t>(0));
  size_t offset = sourceOffset - searchRegionOffset;
  String16 searchArea = script.source().substring(
      searchRegionOffset, offset + kBreakpointHintMaxSearchOffset);

  size_t nextMatch = searchArea.find(hint, offset);
  size_t prevMatch = searchArea.reverseFind(hint, offset);
  if (nextMatch == String16::kNotFound && prevMatch == String16::kNotFound) {
    return;
  }
  size_t bestMatch;
  if (nextMatch == String16::kNotFound) {
    bestMatch = prevMatch;
  } else if (prevMatch == String16::kNotFound) {
    bestMatch = nextMatch;
  } else {
    bestMatch = nextMatch - offset < offset - prevMatch ? nextMatch : prevMatch;
  }
  bestMatch += searchRegionOffset;
  v8::debug::Location hintPosition =
      script.location(static_cast<int>(bestMatch));
  if (hintPosition.IsEmpty()) return;
  breakpoint->line_number = hintPosition.GetLineNumber();
  breakpoint->column_number = hintPosition.GetColumnNumber();
}

String16 breakLocationType(v8::debug::BreakLocationType type) {
  switch (type) {
    case v8::debug::kCallBreakLocation:
      return protocol::Debugger::BreakLocation::TypeEnum::Call;
    case v8::debug::kReturnBreakLocation:
      return protocol::Debugger::BreakLocation::TypeEnum::Return;
    case v8::debug::kDebuggerStatementBreakLocation:
      return protocol::Debugger::BreakLocation::TypeEnum::DebuggerStatement;
    case v8::debug::kCommonBreakLocation:
      return String16();
  }
  return String16();
}

}  // namespace

V8DebuggerAgentImpl::V8DebuggerAgentImpl(
    V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel,
    protocol::DictionaryValue* state)
    : m_inspector(session->inspector()),
      m_debugger(m_inspector->debugger()),
      m_session(session),
      m_enabled(false),
      m_state(state),
      m_frontend(frontendChannel),
      m_isolate(m_inspector->isolate()) {}

V8DebuggerAgentImpl::~V8DebuggerAgentImpl() {}

void V8DebuggerAgentImpl::enableImpl() {
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

Response V8DebuggerAgentImpl::enable() {
  if (enabled()) return Response::OK();

  if (!m_inspector->client()->canExecuteScripts(m_session->contextGroupId()))
    return Response::Error("Script execution is prohibited");

  enableImpl();
  return Response::OK();
}

Response V8DebuggerAgentImpl::disable() {
  if (!enabled()) return Response::OK();

  m_state->setObject(DebuggerAgentState::javaScriptBreakpoints,
                     protocol::DictionaryValue::create());
  m_state->setInteger(DebuggerAgentState::pauseOnExceptionsState,
                      v8::debug::NoBreakOnException);
  m_state->setInteger(DebuggerAgentState::asyncCallStackDepth, 0);

  if (isPaused()) m_debugger->continueProgram(m_session->contextGroupId());
  m_debugger->disable();
  JavaScriptCallFrames emptyCallFrames;
  m_pausedCallFrames.swap(emptyCallFrames);
  m_blackboxedPositions.clear();
  m_blackboxPattern.reset();
  resetBlackboxedStateCache();
  m_scripts.clear();
  m_breakpointIdToDebuggerBreakpointIds.clear();
  m_debugger->setAsyncCallStackDepth(this, 0);
  clearBreakDetails();
  m_skipAllPauses = false;
  m_state->setBoolean(DebuggerAgentState::skipAllPauses, false);
  m_state->remove(DebuggerAgentState::blackboxPattern);
  m_enabled = false;
  m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
  return Response::OK();
}

void V8DebuggerAgentImpl::restore() {
  DCHECK(!m_enabled);
  if (!m_state->booleanProperty(DebuggerAgentState::debuggerEnabled, false))
    return;
  if (!m_inspector->client()->canExecuteScripts(m_session->contextGroupId()))
    return;

  enableImpl();

  int pauseState = v8::debug::NoBreakOnException;
  m_state->getInteger(DebuggerAgentState::pauseOnExceptionsState, &pauseState);
  setPauseOnExceptionsImpl(pauseState);

  m_skipAllPauses =
      m_state->booleanProperty(DebuggerAgentState::skipAllPauses, false);

  int asyncCallStackDepth = 0;
  m_state->getInteger(DebuggerAgentState::asyncCallStackDepth,
                      &asyncCallStackDepth);
  m_debugger->setAsyncCallStackDepth(this, asyncCallStackDepth);

  String16 blackboxPattern;
  if (m_state->getString(DebuggerAgentState::blackboxPattern,
                         &blackboxPattern)) {
    setBlackboxPattern(blackboxPattern);
  }
}

Response V8DebuggerAgentImpl::setBreakpointsActive(bool active) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  m_debugger->setBreakpointsActivated(active);
  if (!active && !m_breakReason.empty()) {
    clearBreakDetails();
    m_debugger->setPauseOnNextStatement(false, m_session->contextGroupId());
  }
  return Response::OK();
}

Response V8DebuggerAgentImpl::setSkipAllPauses(bool skip) {
  m_state->setBoolean(DebuggerAgentState::skipAllPauses, skip);
  m_skipAllPauses = skip;
  return Response::OK();
}

static std::unique_ptr<protocol::DictionaryValue>
buildObjectForBreakpointCookie(const String16& url, int lineNumber,
                               int columnNumber, const String16& condition,
                               bool isRegex, const String16& hint) {
  std::unique_ptr<protocol::DictionaryValue> breakpointObject =
      protocol::DictionaryValue::create();
  breakpointObject->setString(DebuggerAgentState::url, url);
  breakpointObject->setInteger(DebuggerAgentState::lineNumber, lineNumber);
  breakpointObject->setInteger(DebuggerAgentState::columnNumber, columnNumber);
  breakpointObject->setString(DebuggerAgentState::condition, condition);
  breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
  if (!hint.isEmpty()) {
    breakpointObject->setString(DebuggerAgentState::hint, hint);
  }
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

Response V8DebuggerAgentImpl::setBreakpointByUrl(
    int lineNumber, Maybe<String16> optionalURL,
    Maybe<String16> optionalURLRegex, Maybe<int> optionalColumnNumber,
    Maybe<String16> optionalCondition, String16* outBreakpointId,
    std::unique_ptr<protocol::Array<protocol::Debugger::Location>>* locations) {
  *locations = Array<protocol::Debugger::Location>::create();
  if (optionalURL.isJust() == optionalURLRegex.isJust())
    return Response::Error("Either url or urlRegex must be specified.");

  String16 url = optionalURL.isJust() ? optionalURL.fromJust()
                                      : optionalURLRegex.fromJust();
  int columnNumber = 0;
  if (optionalColumnNumber.isJust()) {
    columnNumber = optionalColumnNumber.fromJust();
    if (columnNumber < 0) return Response::Error("Incorrect column number");
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
  if (breakpointsCookie->get(breakpointId))
    return Response::Error("Breakpoint at specified location already exists.");

  String16 hint;
  ScriptBreakpoint breakpoint(String16(), lineNumber, columnNumber, condition);
  for (const auto& script : m_scripts) {
    if (!matches(m_inspector, script.second->sourceURL(), url, isRegex))
      continue;
    breakpoint.script_id = script.first;
    std::unique_ptr<protocol::Debugger::Location> location =
        resolveBreakpoint(breakpointId, breakpoint, UserBreakpointSource, hint);
    if (!isRegex) hint = breakpointHint(*script.second, breakpoint);
    if (location) (*locations)->addItem(std::move(location));
  }

  breakpointsCookie->setObject(
      breakpointId,
      buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition,
                                     isRegex, hint));

  *outBreakpointId = breakpointId;
  return Response::OK();
}

Response V8DebuggerAgentImpl::setBreakpoint(
    std::unique_ptr<protocol::Debugger::Location> location,
    Maybe<String16> optionalCondition, String16* outBreakpointId,
    std::unique_ptr<protocol::Debugger::Location>* actualLocation) {
  ScriptBreakpoint breakpoint(
      location->getScriptId(), location->getLineNumber(),
      location->getColumnNumber(0), optionalCondition.fromMaybe(String16()));

  String16 breakpointId =
      generateBreakpointId(breakpoint, UserBreakpointSource);
  if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) !=
      m_breakpointIdToDebuggerBreakpointIds.end()) {
    return Response::Error("Breakpoint at specified location already exists.");
  }
  *actualLocation = resolveBreakpoint(
      breakpointId, breakpoint, UserBreakpointSource, /* hint */ String16());
  if (!*actualLocation) return Response::Error("Could not resolve breakpoint");
  *outBreakpointId = breakpointId;
  return Response::OK();
}

Response V8DebuggerAgentImpl::removeBreakpoint(const String16& breakpointId) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  protocol::DictionaryValue* breakpointsCookie =
      m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
  if (breakpointsCookie) breakpointsCookie->remove(breakpointId);
  removeBreakpointImpl(breakpointId);
  return Response::OK();
}

void V8DebuggerAgentImpl::removeBreakpointImpl(const String16& breakpointId) {
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

Response V8DebuggerAgentImpl::getPossibleBreakpoints(
    std::unique_ptr<protocol::Debugger::Location> start,
    Maybe<protocol::Debugger::Location> end, Maybe<bool> restrictToFunction,
    std::unique_ptr<protocol::Array<protocol::Debugger::BreakLocation>>*
        locations) {
  String16 scriptId = start->getScriptId();

  if (start->getLineNumber() < 0 || start->getColumnNumber(0) < 0)
    return Response::Error(
        "start.lineNumber and start.columnNumber should be >= 0");

  v8::debug::Location v8Start(start->getLineNumber(),
                              start->getColumnNumber(0));
  v8::debug::Location v8End;
  if (end.isJust()) {
    if (end.fromJust()->getScriptId() != scriptId)
      return Response::Error("Locations should contain the same scriptId");
    int line = end.fromJust()->getLineNumber();
    int column = end.fromJust()->getColumnNumber(0);
    if (line < 0 || column < 0)
      return Response::Error(
          "end.lineNumber and end.columnNumber should be >= 0");
    v8End = v8::debug::Location(line, column);
  }
  auto it = m_scripts.find(scriptId);
  if (it == m_scripts.end()) return Response::Error("Script not found");
  std::vector<v8::debug::BreakLocation> v8Locations;
  {
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Context> debuggerContext =
        v8::debug::GetDebugContext(m_isolate);
    v8::Context::Scope contextScope(debuggerContext);
    v8::TryCatch tryCatch(m_isolate);
    it->second->getPossibleBreakpoints(
        v8Start, v8End, restrictToFunction.fromMaybe(false), &v8Locations);
  }

  *locations = protocol::Array<protocol::Debugger::BreakLocation>::create();
  for (size_t i = 0; i < v8Locations.size(); ++i) {
    std::unique_ptr<protocol::Debugger::BreakLocation> breakLocation =
        protocol::Debugger::BreakLocation::create()
            .setScriptId(scriptId)
            .setLineNumber(v8Locations[i].GetLineNumber())
            .setColumnNumber(v8Locations[i].GetColumnNumber())
            .build();
    if (v8Locations[i].type() != v8::debug::kCommonBreakLocation) {
      breakLocation->setType(breakLocationType(v8Locations[i].type()));
    }
    (*locations)->addItem(std::move(breakLocation));
  }
  return Response::OK();
}

Response V8DebuggerAgentImpl::continueToLocation(
    std::unique_ptr<protocol::Debugger::Location> location,
    Maybe<String16> targetCallFrames) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  return m_debugger->continueToLocation(
      m_session->contextGroupId(), std::move(location),
      targetCallFrames.fromMaybe(
          protocol::Debugger::ContinueToLocation::TargetCallFramesEnum::Any));
}

bool V8DebuggerAgentImpl::isFunctionBlackboxed(const String16& scriptId,
                                               const v8::debug::Location& start,
                                               const v8::debug::Location& end) {
  ScriptsMap::iterator it = m_scripts.find(scriptId);
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
  auto itBlackboxedPositions = m_blackboxedPositions.find(scriptId);
  if (itBlackboxedPositions == m_blackboxedPositions.end()) return false;

  const std::vector<std::pair<int, int>>& ranges =
      itBlackboxedPositions->second;
  auto itStartRange = std::lower_bound(
      ranges.begin(), ranges.end(),
      std::make_pair(start.GetLineNumber(), start.GetColumnNumber()),
      positionComparator);
  auto itEndRange = std::lower_bound(
      itStartRange, ranges.end(),
      std::make_pair(end.GetLineNumber(), end.GetColumnNumber()),
      positionComparator);
  // Ranges array contains positions in script where blackbox state is changed.
  // [(0,0) ... ranges[0]) isn't blackboxed, [ranges[0] ... ranges[1]) is
  // blackboxed...
  return itStartRange == itEndRange &&
         std::distance(ranges.begin(), itStartRange) % 2;
}

std::unique_ptr<protocol::Debugger::Location>
V8DebuggerAgentImpl::resolveBreakpoint(const String16& breakpointId,
                                       const ScriptBreakpoint& breakpoint,
                                       BreakpointSource source,
                                       const String16& hint) {
  v8::HandleScope handles(m_isolate);
  DCHECK(enabled());
  // FIXME: remove these checks once crbug.com/520702 is resolved.
  CHECK(!breakpointId.isEmpty());
  CHECK(!breakpoint.script_id.isEmpty());
  ScriptsMap::iterator scriptIterator = m_scripts.find(breakpoint.script_id);
  if (scriptIterator == m_scripts.end()) return nullptr;
  if (breakpoint.line_number < scriptIterator->second->startLine() ||
      scriptIterator->second->endLine() < breakpoint.line_number)
    return nullptr;

  // Translate from protocol location to v8 location for the debugger.
  ScriptBreakpoint translatedBreakpoint = breakpoint;
  adjustBreakpointLocation(*scriptIterator->second, hint,
                           &translatedBreakpoint);
  m_debugger->wasmTranslation()->TranslateProtocolLocationToWasmScriptLocation(
      &translatedBreakpoint.script_id, &translatedBreakpoint.line_number,
      &translatedBreakpoint.column_number);

  int actualLineNumber;
  int actualColumnNumber;
  String16 debuggerBreakpointId = m_debugger->setBreakpoint(
      translatedBreakpoint, &actualLineNumber, &actualColumnNumber);
  if (debuggerBreakpointId.isEmpty()) return nullptr;

  // Translate back from v8 location to protocol location for the return value.
  m_debugger->wasmTranslation()->TranslateWasmScriptLocationToProtocolLocation(
      &translatedBreakpoint.script_id, &actualLineNumber, &actualColumnNumber);

  m_serverBreakpoints[debuggerBreakpointId] =
      std::make_pair(breakpointId, source);
  CHECK(!breakpointId.isEmpty());

  m_breakpointIdToDebuggerBreakpointIds[breakpointId].push_back(
      debuggerBreakpointId);
  return buildProtocolLocation(translatedBreakpoint.script_id, actualLineNumber,
                               actualColumnNumber);
}

Response V8DebuggerAgentImpl::searchInContent(
    const String16& scriptId, const String16& query,
    Maybe<bool> optionalCaseSensitive, Maybe<bool> optionalIsRegex,
    std::unique_ptr<Array<protocol::Debugger::SearchMatch>>* results) {
  v8::HandleScope handles(m_isolate);
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end())
    return Response::Error("No script for id: " + scriptId);

  std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> matches =
      searchInTextByLinesImpl(m_session, it->second->source(), query,
                              optionalCaseSensitive.fromMaybe(false),
                              optionalIsRegex.fromMaybe(false));
  *results = protocol::Array<protocol::Debugger::SearchMatch>::create();
  for (size_t i = 0; i < matches.size(); ++i)
    (*results)->addItem(std::move(matches[i]));
  return Response::OK();
}

Response V8DebuggerAgentImpl::setScriptSource(
    const String16& scriptId, const String16& newContent, Maybe<bool> dryRun,
    Maybe<protocol::Array<protocol::Debugger::CallFrame>>* newCallFrames,
    Maybe<bool>* stackChanged,
    Maybe<protocol::Runtime::StackTrace>* asyncStackTrace,
    Maybe<protocol::Runtime::ExceptionDetails>* optOutCompileError) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);

  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end()) {
    return Response::Error("No script with given id found");
  }
  if (it->second->isModule()) {
    // TODO(kozyatinskiy): LiveEdit should support ES6 module
    return Response::Error("Editing module's script is not supported.");
  }

  v8::HandleScope handles(m_isolate);
  v8::Local<v8::String> newSource = toV8String(m_isolate, newContent);
  bool compileError = false;
  Response response = m_debugger->setScriptSource(
      scriptId, newSource, dryRun.fromMaybe(false), optOutCompileError,
      &m_pausedCallFrames, stackChanged, &compileError);
  if (!response.isSuccess() || compileError) return response;

  it->second->setSource(newContent);
  std::unique_ptr<Array<CallFrame>> callFrames;
  response = currentCallFrames(&callFrames);
  if (!response.isSuccess()) return response;
  *newCallFrames = std::move(callFrames);
  *asyncStackTrace = currentAsyncStackTrace();
  return Response::OK();
}

Response V8DebuggerAgentImpl::restartFrame(
    const String16& callFrameId,
    std::unique_ptr<Array<CallFrame>>* newCallFrames,
    Maybe<protocol::Runtime::StackTrace>* asyncStackTrace) {
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  InjectedScript::CallFrameScope scope(m_inspector, m_session->contextGroupId(),
                                       callFrameId);
  Response response = scope.initialize();
  if (!response.isSuccess()) return response;
  if (scope.frameOrdinal() >= m_pausedCallFrames.size())
    return Response::Error("Could not find call frame with given id");

  v8::Local<v8::Value> resultValue;
  v8::Local<v8::Boolean> result;
  if (!m_pausedCallFrames[scope.frameOrdinal()]->restart().ToLocal(
          &resultValue) ||
      scope.tryCatch().HasCaught() ||
      !resultValue->ToBoolean(scope.context()).ToLocal(&result) ||
      !result->Value()) {
    return Response::InternalError();
  }
  JavaScriptCallFrames frames = m_debugger->currentCallFrames();
  m_pausedCallFrames.swap(frames);

  response = currentCallFrames(newCallFrames);
  if (!response.isSuccess()) return response;
  *asyncStackTrace = currentAsyncStackTrace();
  return Response::OK();
}

Response V8DebuggerAgentImpl::getScriptSource(const String16& scriptId,
                                              String16* scriptSource) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end())
    return Response::Error("No script for id: " + scriptId);
  *scriptSource = it->second->source();
  return Response::OK();
}

void V8DebuggerAgentImpl::pushBreakDetails(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> breakAuxData) {
  m_breakReason.push_back(std::make_pair(breakReason, std::move(breakAuxData)));
}

void V8DebuggerAgentImpl::popBreakDetails() {
  if (m_breakReason.empty()) return;
  m_breakReason.pop_back();
}

void V8DebuggerAgentImpl::clearBreakDetails() {
  std::vector<BreakReason> emptyBreakReason;
  m_breakReason.swap(emptyBreakReason);
}

void V8DebuggerAgentImpl::schedulePauseOnNextStatement(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() || isPaused() || !m_debugger->breakpointsActivated()) return;
  if (m_breakReason.empty()) {
    m_debugger->setPauseOnNextStatement(true, m_session->contextGroupId());
  }
  pushBreakDetails(breakReason, std::move(data));
}

void V8DebuggerAgentImpl::cancelPauseOnNextStatement() {
  if (!enabled() || isPaused() || !m_debugger->breakpointsActivated()) return;
  if (m_breakReason.size() == 1) {
    m_debugger->setPauseOnNextStatement(false, m_session->contextGroupId());
  }
  popBreakDetails();
}

Response V8DebuggerAgentImpl::pause() {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  if (isPaused()) return Response::OK();
  if (m_breakReason.empty()) {
    m_debugger->setPauseOnNextStatement(true, m_session->contextGroupId());
  }
  pushBreakDetails(protocol::Debugger::Paused::ReasonEnum::Other, nullptr);
  return Response::OK();
}

Response V8DebuggerAgentImpl::resume() {
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->continueProgram(m_session->contextGroupId());
  return Response::OK();
}

Response V8DebuggerAgentImpl::stepOver() {
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->stepOverStatement(m_session->contextGroupId());
  return Response::OK();
}

Response V8DebuggerAgentImpl::stepInto() {
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->stepIntoStatement(m_session->contextGroupId());
  return Response::OK();
}

Response V8DebuggerAgentImpl::stepOut() {
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->stepOutOfFunction(m_session->contextGroupId());
  return Response::OK();
}

void V8DebuggerAgentImpl::scheduleStepIntoAsync(
    std::unique_ptr<ScheduleStepIntoAsyncCallback> callback) {
  if (!isPaused()) {
    callback->sendFailure(Response::Error(kDebuggerNotPaused));
    return;
  }
  m_debugger->scheduleStepIntoAsync(std::move(callback),
                                    m_session->contextGroupId());
}

Response V8DebuggerAgentImpl::setPauseOnExceptions(
    const String16& stringPauseState) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  v8::debug::ExceptionBreakState pauseState;
  if (stringPauseState == "none") {
    pauseState = v8::debug::NoBreakOnException;
  } else if (stringPauseState == "all") {
    pauseState = v8::debug::BreakOnAnyException;
  } else if (stringPauseState == "uncaught") {
    pauseState = v8::debug::BreakOnUncaughtException;
  } else {
    return Response::Error("Unknown pause on exceptions mode: " +
                           stringPauseState);
  }
  setPauseOnExceptionsImpl(pauseState);
  return Response::OK();
}

void V8DebuggerAgentImpl::setPauseOnExceptionsImpl(int pauseState) {
  m_debugger->setPauseOnExceptionsState(
      static_cast<v8::debug::ExceptionBreakState>(pauseState));
  m_state->setInteger(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

Response V8DebuggerAgentImpl::evaluateOnCallFrame(
    const String16& callFrameId, const String16& expression,
    Maybe<String16> objectGroup, Maybe<bool> includeCommandLineAPI,
    Maybe<bool> silent, Maybe<bool> returnByValue, Maybe<bool> generatePreview,
    Maybe<bool> throwOnSideEffect, std::unique_ptr<RemoteObject>* result,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  InjectedScript::CallFrameScope scope(m_inspector, m_session->contextGroupId(),
                                       callFrameId);
  Response response = scope.initialize();
  if (!response.isSuccess()) return response;
  if (scope.frameOrdinal() >= m_pausedCallFrames.size())
    return Response::Error("Could not find call frame with given id");

  if (includeCommandLineAPI.fromMaybe(false)) scope.installCommandLineAPI();
  if (silent.fromMaybe(false)) scope.ignoreExceptionsAndMuteConsole();

  v8::MaybeLocal<v8::Value> maybeResultValue =
      m_pausedCallFrames[scope.frameOrdinal()]->evaluate(
          toV8String(m_isolate, expression),
          throwOnSideEffect.fromMaybe(false));

  // Re-initialize after running client's code, as it could have destroyed
  // context or session.
  response = scope.initialize();
  if (!response.isSuccess()) return response;
  return scope.injectedScript()->wrapEvaluateResult(
      maybeResultValue, scope.tryCatch(), objectGroup.fromMaybe(""),
      returnByValue.fromMaybe(false), generatePreview.fromMaybe(false), result,
      exceptionDetails);
}

Response V8DebuggerAgentImpl::setVariableValue(
    int scopeNumber, const String16& variableName,
    std::unique_ptr<protocol::Runtime::CallArgument> newValueArgument,
    const String16& callFrameId) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  if (!isPaused()) return Response::Error(kDebuggerNotPaused);
  InjectedScript::CallFrameScope scope(m_inspector, m_session->contextGroupId(),
                                       callFrameId);
  Response response = scope.initialize();
  if (!response.isSuccess()) return response;
  v8::Local<v8::Value> newValue;
  response = scope.injectedScript()->resolveCallArgument(newValueArgument.get(),
                                                         &newValue);
  if (!response.isSuccess()) return response;

  if (scope.frameOrdinal() >= m_pausedCallFrames.size())
    return Response::Error("Could not find call frame with given id");
  v8::MaybeLocal<v8::Value> result =
      m_pausedCallFrames[scope.frameOrdinal()]->setVariableValue(
          scopeNumber, toV8String(m_isolate, variableName), newValue);
  if (scope.tryCatch().HasCaught() || result.IsEmpty())
    return Response::InternalError();
  return Response::OK();
}

Response V8DebuggerAgentImpl::setAsyncCallStackDepth(int depth) {
  if (!enabled()) return Response::Error(kDebuggerNotEnabled);
  m_state->setInteger(DebuggerAgentState::asyncCallStackDepth, depth);
  m_debugger->setAsyncCallStackDepth(this, depth);
  return Response::OK();
}

Response V8DebuggerAgentImpl::setBlackboxPatterns(
    std::unique_ptr<protocol::Array<String16>> patterns) {
  if (!patterns->length()) {
    m_blackboxPattern = nullptr;
    resetBlackboxedStateCache();
    m_state->remove(DebuggerAgentState::blackboxPattern);
    return Response::OK();
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
  Response response = setBlackboxPattern(pattern);
  if (!response.isSuccess()) return response;
  resetBlackboxedStateCache();
  m_state->setString(DebuggerAgentState::blackboxPattern, pattern);
  return Response::OK();
}

Response V8DebuggerAgentImpl::setBlackboxPattern(const String16& pattern) {
  std::unique_ptr<V8Regex> regex(new V8Regex(
      m_inspector, pattern, true /** caseSensitive */, false /** multiline */));
  if (!regex->isValid())
    return Response::Error("Pattern parser error: " + regex->errorMessage());
  m_blackboxPattern = std::move(regex);
  return Response::OK();
}

void V8DebuggerAgentImpl::resetBlackboxedStateCache() {
  for (const auto& it : m_scripts) {
    it.second->resetBlackboxedStateCache();
  }
}

Response V8DebuggerAgentImpl::setBlackboxedRanges(
    const String16& scriptId,
    std::unique_ptr<protocol::Array<protocol::Debugger::ScriptPosition>>
        inPositions) {
  auto it = m_scripts.find(scriptId);
  if (it == m_scripts.end())
    return Response::Error("No script with passed id.");

  if (!inPositions->length()) {
    m_blackboxedPositions.erase(scriptId);
    it->second->resetBlackboxedStateCache();
    return Response::OK();
  }

  std::vector<std::pair<int, int>> positions;
  positions.reserve(inPositions->length());
  for (size_t i = 0; i < inPositions->length(); ++i) {
    protocol::Debugger::ScriptPosition* position = inPositions->get(i);
    if (position->getLineNumber() < 0)
      return Response::Error("Position missing 'line' or 'line' < 0.");
    if (position->getColumnNumber() < 0)
      return Response::Error("Position missing 'column' or 'column' < 0.");
    positions.push_back(
        std::make_pair(position->getLineNumber(), position->getColumnNumber()));
  }

  for (size_t i = 1; i < positions.size(); ++i) {
    if (positions[i - 1].first < positions[i].first) continue;
    if (positions[i - 1].first == positions[i].first &&
        positions[i - 1].second < positions[i].second)
      continue;
    return Response::Error(
        "Input positions array is not sorted or contains duplicate values.");
  }

  m_blackboxedPositions[scriptId] = positions;
  it->second->resetBlackboxedStateCache();
  return Response::OK();
}

Response V8DebuggerAgentImpl::currentCallFrames(
    std::unique_ptr<Array<CallFrame>>* result) {
  if (!isPaused()) {
    *result = Array<CallFrame>::create();
    return Response::OK();
  }
  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Context> debuggerContext =
      v8::debug::GetDebugContext(m_isolate);
  v8::Context::Scope contextScope(debuggerContext);

  v8::Local<v8::Array> objects = v8::Array::New(m_isolate);

  for (size_t frameOrdinal = 0; frameOrdinal < m_pausedCallFrames.size();
       ++frameOrdinal) {
    const std::unique_ptr<JavaScriptCallFrame>& currentCallFrame =
        m_pausedCallFrames[frameOrdinal];

    v8::Local<v8::Object> details;
    if (!currentCallFrame->details().ToLocal(&details))
      return Response::InternalError();

    int contextId = currentCallFrame->contextId();

    InjectedScript* injectedScript = nullptr;
    if (contextId) m_session->findInjectedScript(contextId, injectedScript);

    String16 callFrameId =
        RemoteCallFrameId::serialize(contextId, static_cast<int>(frameOrdinal));
    if (!details
             ->Set(debuggerContext,
                   toV8StringInternalized(m_isolate, "callFrameId"),
                   toV8String(m_isolate, callFrameId))
             .FromMaybe(false)) {
      return Response::InternalError();
    }

    if (injectedScript) {
      v8::Local<v8::Value> scopeChain;
      if (!details
               ->Get(debuggerContext,
                     toV8StringInternalized(m_isolate, "scopeChain"))
               .ToLocal(&scopeChain) ||
          !scopeChain->IsArray()) {
        return Response::InternalError();
      }
      v8::Local<v8::Array> scopeChainArray = scopeChain.As<v8::Array>();
      Response response = injectedScript->wrapPropertyInArray(
          scopeChainArray, toV8StringInternalized(m_isolate, "object"),
          kBacktraceObjectGroup);
      if (!response.isSuccess()) return response;
      response = injectedScript->wrapObjectProperty(
          details, toV8StringInternalized(m_isolate, "this"),
          kBacktraceObjectGroup);
      if (!response.isSuccess()) return response;
      if (details
              ->Has(debuggerContext,
                    toV8StringInternalized(m_isolate, "returnValue"))
              .FromMaybe(false)) {
        response = injectedScript->wrapObjectProperty(
            details, toV8StringInternalized(m_isolate, "returnValue"),
            kBacktraceObjectGroup);
        if (!response.isSuccess()) return response;
      }
    } else {
      if (!details
               ->Set(debuggerContext,
                     toV8StringInternalized(m_isolate, "scopeChain"),
                     v8::Array::New(m_isolate, 0))
               .FromMaybe(false)) {
        return Response::InternalError();
      }
      v8::Local<v8::Object> remoteObject = v8::Object::New(m_isolate);
      if (!remoteObject
               ->Set(debuggerContext, toV8StringInternalized(m_isolate, "type"),
                     toV8StringInternalized(m_isolate, "undefined"))
               .FromMaybe(false)) {
        return Response::InternalError();
      }
      if (!details
               ->Set(debuggerContext, toV8StringInternalized(m_isolate, "this"),
                     remoteObject)
               .FromMaybe(false)) {
        return Response::InternalError();
      }
      if (!details
               ->Delete(debuggerContext,
                        toV8StringInternalized(m_isolate, "returnValue"))
               .FromMaybe(false)) {
        return Response::InternalError();
      }
    }

    if (!objects->Set(debuggerContext, static_cast<int>(frameOrdinal), details)
             .FromMaybe(false)) {
      return Response::InternalError();
    }
  }

  std::unique_ptr<protocol::Value> protocolValue;
  Response response = toProtocolValue(debuggerContext, objects, &protocolValue);
  if (!response.isSuccess()) return response;
  protocol::ErrorSupport errorSupport;
  *result = Array<CallFrame>::fromValue(protocolValue.get(), &errorSupport);
  if (!*result) return Response::Error(errorSupport.errors());
  TranslateWasmStackTraceLocations(result->get(),
                                   m_debugger->wasmTranslation());
  return Response::OK();
}

std::unique_ptr<protocol::Runtime::StackTrace>
V8DebuggerAgentImpl::currentAsyncStackTrace() {
  std::shared_ptr<AsyncStackTrace> asyncParent =
      m_debugger->currentAsyncParent();
  if (!asyncParent) return nullptr;
  return asyncParent->buildInspectorObject(
      m_debugger->currentAsyncCreation().get(),
      m_debugger->maxAsyncCallChainDepth() - 1);
}

bool V8DebuggerAgentImpl::isPaused() const { return m_debugger->isPaused(); }

void V8DebuggerAgentImpl::didParseSource(
    std::unique_ptr<V8DebuggerScript> script, bool success) {
  v8::HandleScope handles(m_isolate);
  String16 scriptSource = script->source();
  if (!success) script->setSourceURL(findSourceURL(scriptSource, false));
  if (!success)
    script->setSourceMappingURL(findSourceMapURL(scriptSource, false));

  int contextId = script->executionContextId();
  int contextGroupId = m_inspector->contextGroupId(contextId);
  InspectedContext* inspected =
      m_inspector->getContext(contextGroupId, contextId);
  std::unique_ptr<protocol::DictionaryValue> executionContextAuxData;
  if (inspected) {
    // Script reused between different groups/sessions can have a stale
    // execution context id.
    executionContextAuxData = protocol::DictionaryValue::cast(
        protocol::StringUtil::parseJSON(inspected->auxData()));
  }
  bool isLiveEdit = script->isLiveEdit();
  bool hasSourceURL = script->hasSourceURL();
  bool isModule = script->isModule();
  String16 scriptId = script->scriptId();
  String16 scriptURL = script->sourceURL();

  m_scripts[scriptId] = std::move(script);

  ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
  DCHECK(scriptIterator != m_scripts.end());
  V8DebuggerScript* scriptRef = scriptIterator->second.get();
  // V8 could create functions for parsed scripts before reporting and asks
  // inspector about blackboxed state, we should reset state each time when we
  // make any change that change isFunctionBlackboxed output - adding parsed
  // script is changing.
  scriptRef->resetBlackboxedStateCache();

  Maybe<String16> sourceMapURLParam = scriptRef->sourceMappingURL();
  Maybe<protocol::DictionaryValue> executionContextAuxDataParam(
      std::move(executionContextAuxData));
  const bool* isLiveEditParam = isLiveEdit ? &isLiveEdit : nullptr;
  const bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : nullptr;
  const bool* isModuleParam = isModule ? &isModule : nullptr;
  std::unique_ptr<V8StackTraceImpl> stack =
      V8StackTraceImpl::capture(m_inspector->debugger(), contextGroupId, 1);
  std::unique_ptr<protocol::Runtime::StackTrace> stackTrace =
      stack && !stack->isEmpty() ? stack->buildInspectorObjectImpl() : nullptr;
  if (success) {
    m_frontend.scriptParsed(
        scriptId, scriptURL, scriptRef->startLine(), scriptRef->startColumn(),
        scriptRef->endLine(), scriptRef->endColumn(), contextId,
        scriptRef->hash(), std::move(executionContextAuxDataParam),
        isLiveEditParam, std::move(sourceMapURLParam), hasSourceURLParam,
        isModuleParam, static_cast<int>(scriptRef->source().length()),
        std::move(stackTrace));
  } else {
    m_frontend.scriptFailedToParse(
        scriptId, scriptURL, scriptRef->startLine(), scriptRef->startColumn(),
        scriptRef->endLine(), scriptRef->endColumn(), contextId,
        scriptRef->hash(), std::move(executionContextAuxDataParam),
        std::move(sourceMapURLParam), hasSourceURLParam, isModuleParam,
        static_cast<int>(scriptRef->source().length()), std::move(stackTrace));
  }

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
    breakpoint.script_id = scriptId;
    breakpointObject->getInteger(DebuggerAgentState::lineNumber,
                                 &breakpoint.line_number);
    breakpointObject->getInteger(DebuggerAgentState::columnNumber,
                                 &breakpoint.column_number);
    breakpointObject->getString(DebuggerAgentState::condition,
                                &breakpoint.condition);
    String16 hint;
    bool hasHint = breakpointObject->getString(DebuggerAgentState::hint, &hint);
    std::unique_ptr<protocol::Debugger::Location> location =
        resolveBreakpoint(cookie.first, breakpoint, UserBreakpointSource, hint);
    if (!hasHint) {
      hint = breakpointHint(*scriptRef, breakpoint);
      if (!hint.isEmpty())
        breakpointObject->setString(DebuggerAgentState::hint, hint);
    }
    if (location)
      m_frontend.breakpointResolved(cookie.first, std::move(location));
  }
}

void V8DebuggerAgentImpl::didPause(int contextId,
                                   v8::Local<v8::Value> exception,
                                   const std::vector<String16>& hitBreakpoints,
                                   bool isPromiseRejection, bool isUncaught,
                                   bool isOOMBreak) {
  JavaScriptCallFrames frames = m_debugger->currentCallFrames();
  m_pausedCallFrames.swap(frames);
  v8::HandleScope handles(m_isolate);

  std::vector<BreakReason> hitReasons;

  if (isOOMBreak) {
    hitReasons.push_back(
        std::make_pair(protocol::Debugger::Paused::ReasonEnum::OOM, nullptr));
  } else if (!exception.IsEmpty()) {
    InjectedScript* injectedScript = nullptr;
    m_session->findInjectedScript(contextId, injectedScript);
    if (injectedScript) {
      String16 breakReason =
          isPromiseRejection
              ? protocol::Debugger::Paused::ReasonEnum::PromiseRejection
              : protocol::Debugger::Paused::ReasonEnum::Exception;
      std::unique_ptr<protocol::Runtime::RemoteObject> obj;
      injectedScript->wrapObject(exception, kBacktraceObjectGroup, false, false,
                                 &obj);
      std::unique_ptr<protocol::DictionaryValue> breakAuxData;
      if (obj) {
        breakAuxData = obj->toValue();
        breakAuxData->setBoolean("uncaught", isUncaught);
      } else {
        breakAuxData = nullptr;
      }
      hitReasons.push_back(
          std::make_pair(breakReason, std::move(breakAuxData)));
    }
  }

  std::unique_ptr<Array<String16>> hitBreakpointIds = Array<String16>::create();

  bool hasDebugCommandBreakpointReason = false;
  for (const auto& point : hitBreakpoints) {
    DebugServerBreakpointToBreakpointIdAndSourceMap::iterator
        breakpointIterator = m_serverBreakpoints.find(point);
    if (breakpointIterator != m_serverBreakpoints.end()) {
      const String16& localId = breakpointIterator->second.first;
      hitBreakpointIds->addItem(localId);

      BreakpointSource source = breakpointIterator->second.second;
      if (!hasDebugCommandBreakpointReason &&
          source == DebugCommandBreakpointSource) {
        hasDebugCommandBreakpointReason = true;
        hitReasons.push_back(std::make_pair(
            protocol::Debugger::Paused::ReasonEnum::DebugCommand, nullptr));
      }
    }
  }

  for (size_t i = 0; i < m_breakReason.size(); ++i) {
    hitReasons.push_back(std::move(m_breakReason[i]));
  }
  clearBreakDetails();

  String16 breakReason = protocol::Debugger::Paused::ReasonEnum::Other;
  std::unique_ptr<protocol::DictionaryValue> breakAuxData;
  if (hitReasons.size() == 1) {
    breakReason = hitReasons[0].first;
    breakAuxData = std::move(hitReasons[0].second);
  } else if (hitReasons.size() > 1) {
    breakReason = protocol::Debugger::Paused::ReasonEnum::Ambiguous;
    std::unique_ptr<protocol::ListValue> reasons =
        protocol::ListValue::create();
    for (size_t i = 0; i < hitReasons.size(); ++i) {
      std::unique_ptr<protocol::DictionaryValue> reason =
          protocol::DictionaryValue::create();
      reason->setString("reason", hitReasons[i].first);
      if (hitReasons[i].second)
        reason->setObject("auxData", std::move(hitReasons[i].second));
      reasons->pushValue(std::move(reason));
    }
    breakAuxData = protocol::DictionaryValue::create();
    breakAuxData->setArray("reasons", std::move(reasons));
  }

  std::unique_ptr<Array<CallFrame>> protocolCallFrames;
  Response response = currentCallFrames(&protocolCallFrames);
  if (!response.isSuccess()) protocolCallFrames = Array<CallFrame>::create();
  m_frontend.paused(std::move(protocolCallFrames), breakReason,
                    std::move(breakAuxData), std::move(hitBreakpointIds),
                    currentAsyncStackTrace());
}

void V8DebuggerAgentImpl::didContinue() {
  JavaScriptCallFrames emptyCallFrames;
  m_pausedCallFrames.swap(emptyCallFrames);
  clearBreakDetails();
  m_frontend.resumed();
}

void V8DebuggerAgentImpl::breakProgram(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() || !m_debugger->canBreakProgram() || m_skipAllPauses) return;
  std::vector<BreakReason> currentScheduledReason;
  currentScheduledReason.swap(m_breakReason);
  pushBreakDetails(breakReason, std::move(data));
  if (!m_debugger->breakProgram(m_session->contextGroupId())) return;
  popBreakDetails();
  m_breakReason.swap(currentScheduledReason);
  if (!m_breakReason.empty()) {
    m_debugger->setPauseOnNextStatement(true, m_session->contextGroupId());
  }
}

void V8DebuggerAgentImpl::breakProgramOnException(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() ||
      m_debugger->getPauseOnExceptionsState() == v8::debug::NoBreakOnException)
    return;
  breakProgram(breakReason, std::move(data));
}

void V8DebuggerAgentImpl::setBreakpointAt(const String16& scriptId,
                                          int lineNumber, int columnNumber,
                                          BreakpointSource source,
                                          const String16& condition) {
  ScriptBreakpoint breakpoint(scriptId, lineNumber, columnNumber, condition);
  String16 breakpointId = generateBreakpointId(breakpoint, source);
  resolveBreakpoint(breakpointId, breakpoint, source, /* hint */ String16());
}

void V8DebuggerAgentImpl::removeBreakpointAt(const String16& scriptId,
                                             int lineNumber, int columnNumber,
                                             BreakpointSource source) {
  removeBreakpointImpl(generateBreakpointId(
      ScriptBreakpoint(scriptId, lineNumber, columnNumber, String16()),
      source));
}

void V8DebuggerAgentImpl::reset() {
  if (!enabled()) return;
  m_blackboxedPositions.clear();
  resetBlackboxedStateCache();
  m_scripts.clear();
  m_breakpointIdToDebuggerBreakpointIds.clear();
}

}  // namespace v8_inspector
