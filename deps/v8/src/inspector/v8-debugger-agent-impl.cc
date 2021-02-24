// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-agent-impl.h"

#include <algorithm>

#include "../../third_party/inspector_protocol/crdtp/json.h"
#include "include/v8-inspector.h"
#include "src/base/safe_conversions.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/remote-object-id.h"
#include "src/inspector/search-util.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-script.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-regex.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"
#include "src/inspector/v8-value-utils.h"

namespace v8_inspector {

using protocol::Array;
using protocol::Maybe;
using protocol::Debugger::BreakpointId;
using protocol::Debugger::CallFrame;
using protocol::Debugger::Scope;
using protocol::Runtime::ExceptionDetails;
using protocol::Runtime::RemoteObject;
using protocol::Runtime::ScriptId;

namespace InstrumentationEnum =
    protocol::Debugger::SetInstrumentationBreakpoint::InstrumentationEnum;

namespace DebuggerAgentState {
static const char pauseOnExceptionsState[] = "pauseOnExceptionsState";
static const char asyncCallStackDepth[] = "asyncCallStackDepth";
static const char blackboxPattern[] = "blackboxPattern";
static const char debuggerEnabled[] = "debuggerEnabled";
static const char skipAllPauses[] = "skipAllPauses";

static const char breakpointsByRegex[] = "breakpointsByRegex";
static const char breakpointsByUrl[] = "breakpointsByUrl";
static const char breakpointsByScriptHash[] = "breakpointsByScriptHash";
static const char breakpointHints[] = "breakpointHints";
static const char instrumentationBreakpoints[] = "instrumentationBreakpoints";

}  // namespace DebuggerAgentState

static const char kBacktraceObjectGroup[] = "backtrace";
static const char kDebuggerNotEnabled[] = "Debugger agent is not enabled";
static const char kDebuggerNotPaused[] =
    "Can only perform operation while paused.";

static const size_t kBreakpointHintMaxLength = 128;
static const intptr_t kBreakpointHintMaxSearchOffset = 80 * 10;
// Limit the number of breakpoints returned, as we otherwise may exceed
// the maximum length of a message in mojo (see https://crbug.com/1105172).
static const size_t kMaxNumBreakpoints = 1000;

// TODO(1099680): getScriptSource and getWasmBytecode return Wasm wire bytes
// as protocol::Binary, which is encoded as JSON string in the communication
// to the DevTools front-end and hence leads to either crashing the renderer
// that is being debugged or the renderer that's running the front-end if we
// allow arbitrarily big Wasm byte sequences here. Ideally we would find a
// different way to transfer the wire bytes (middle- to long-term), but as a
// short-term solution, we should at least not crash.
static const size_t kWasmBytecodeMaxLength = (v8::String::kMaxLength / 4) * 3;
static const char kWasmBytecodeExceedsTransferLimit[] =
    "WebAssembly bytecode exceeds the transfer limit";

namespace {

enum class BreakpointType {
  kByUrl = 1,
  kByUrlRegex,
  kByScriptHash,
  kByScriptId,
  kDebugCommand,
  kMonitorCommand,
  kBreakpointAtEntry,
  kInstrumentationBreakpoint
};

String16 generateBreakpointId(BreakpointType type,
                              const String16& scriptSelector, int lineNumber,
                              int columnNumber) {
  String16Builder builder;
  builder.appendNumber(static_cast<int>(type));
  builder.append(':');
  builder.appendNumber(lineNumber);
  builder.append(':');
  builder.appendNumber(columnNumber);
  builder.append(':');
  builder.append(scriptSelector);
  return builder.toString();
}

String16 generateBreakpointId(BreakpointType type,
                              v8::Local<v8::Function> function) {
  String16Builder builder;
  builder.appendNumber(static_cast<int>(type));
  builder.append(':');
  builder.appendNumber(v8::debug::GetDebuggingId(function));
  return builder.toString();
}

String16 generateInstrumentationBreakpointId(const String16& instrumentation) {
  String16Builder builder;
  builder.appendNumber(
      static_cast<int>(BreakpointType::kInstrumentationBreakpoint));
  builder.append(':');
  builder.append(instrumentation);
  return builder.toString();
}

bool parseBreakpointId(const String16& breakpointId, BreakpointType* type,
                       String16* scriptSelector = nullptr,
                       int* lineNumber = nullptr, int* columnNumber = nullptr) {
  size_t typeLineSeparator = breakpointId.find(':');
  if (typeLineSeparator == String16::kNotFound) return false;

  int rawType = breakpointId.substring(0, typeLineSeparator).toInteger();
  if (rawType < static_cast<int>(BreakpointType::kByUrl) ||
      rawType > static_cast<int>(BreakpointType::kInstrumentationBreakpoint)) {
    return false;
  }
  if (type) *type = static_cast<BreakpointType>(rawType);
  if (rawType == static_cast<int>(BreakpointType::kDebugCommand) ||
      rawType == static_cast<int>(BreakpointType::kMonitorCommand) ||
      rawType == static_cast<int>(BreakpointType::kBreakpointAtEntry) ||
      rawType == static_cast<int>(BreakpointType::kInstrumentationBreakpoint)) {
    // The script and source position are not encoded in this case.
    return true;
  }

  size_t lineColumnSeparator = breakpointId.find(':', typeLineSeparator + 1);
  if (lineColumnSeparator == String16::kNotFound) return false;
  size_t columnSelectorSeparator =
      breakpointId.find(':', lineColumnSeparator + 1);
  if (columnSelectorSeparator == String16::kNotFound) return false;
  if (scriptSelector) {
    *scriptSelector = breakpointId.substring(columnSelectorSeparator + 1);
  }
  if (lineNumber) {
    *lineNumber = breakpointId
                      .substring(typeLineSeparator + 1,
                                 lineColumnSeparator - typeLineSeparator - 1)
                      .toInteger();
  }
  if (columnNumber) {
    *columnNumber =
        breakpointId
            .substring(lineColumnSeparator + 1,
                       columnSelectorSeparator - lineColumnSeparator - 1)
            .toInteger();
  }
  return true;
}

bool positionComparator(const std::pair<int, int>& a,
                        const std::pair<int, int>& b) {
  if (a.first != b.first) return a.first < b.first;
  return a.second < b.second;
}

String16 breakpointHint(const V8DebuggerScript& script, int lineNumber,
                        int columnNumber) {
  int offset = script.offset(lineNumber, columnNumber);
  if (offset == V8DebuggerScript::kNoOffset) return String16();
  String16 hint =
      script.source(offset, kBreakpointHintMaxLength).stripWhiteSpace();
  for (size_t i = 0; i < hint.length(); ++i) {
    if (hint[i] == '\r' || hint[i] == '\n' || hint[i] == ';') {
      return hint.substring(0, i);
    }
  }
  return hint;
}

void adjustBreakpointLocation(const V8DebuggerScript& script,
                              const String16& hint, int* lineNumber,
                              int* columnNumber) {
  if (*lineNumber < script.startLine() || *lineNumber > script.endLine())
    return;
  if (hint.isEmpty()) return;
  intptr_t sourceOffset = script.offset(*lineNumber, *columnNumber);
  if (sourceOffset == V8DebuggerScript::kNoOffset) return;

  intptr_t searchRegionOffset = std::max(
      sourceOffset - kBreakpointHintMaxSearchOffset, static_cast<intptr_t>(0));
  size_t offset = sourceOffset - searchRegionOffset;
  String16 searchArea = script.source(searchRegionOffset,
                                      offset + kBreakpointHintMaxSearchOffset);

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
  *lineNumber = hintPosition.GetLineNumber();
  *columnNumber = hintPosition.GetColumnNumber();
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

String16 scopeType(v8::debug::ScopeIterator::ScopeType type) {
  switch (type) {
    case v8::debug::ScopeIterator::ScopeTypeGlobal:
      return Scope::TypeEnum::Global;
    case v8::debug::ScopeIterator::ScopeTypeLocal:
      return Scope::TypeEnum::Local;
    case v8::debug::ScopeIterator::ScopeTypeWith:
      return Scope::TypeEnum::With;
    case v8::debug::ScopeIterator::ScopeTypeClosure:
      return Scope::TypeEnum::Closure;
    case v8::debug::ScopeIterator::ScopeTypeCatch:
      return Scope::TypeEnum::Catch;
    case v8::debug::ScopeIterator::ScopeTypeBlock:
      return Scope::TypeEnum::Block;
    case v8::debug::ScopeIterator::ScopeTypeScript:
      return Scope::TypeEnum::Script;
    case v8::debug::ScopeIterator::ScopeTypeEval:
      return Scope::TypeEnum::Eval;
    case v8::debug::ScopeIterator::ScopeTypeModule:
      return Scope::TypeEnum::Module;
    case v8::debug::ScopeIterator::ScopeTypeWasmExpressionStack:
      return Scope::TypeEnum::WasmExpressionStack;
  }
  UNREACHABLE();
  return String16();
}

Response buildScopes(v8::Isolate* isolate, v8::debug::ScopeIterator* iterator,
                     InjectedScript* injectedScript,
                     std::unique_ptr<Array<Scope>>* scopes) {
  *scopes = std::make_unique<Array<Scope>>();
  if (!injectedScript) return Response::Success();
  if (iterator->Done()) return Response::Success();

  String16 scriptId = String16::fromInteger(iterator->GetScriptId());

  for (; !iterator->Done(); iterator->Advance()) {
    std::unique_ptr<RemoteObject> object;
    Response result =
        injectedScript->wrapObject(iterator->GetObject(), kBacktraceObjectGroup,
                                   WrapMode::kNoPreview, &object);
    if (!result.IsSuccess()) return result;

    auto scope = Scope::create()
                     .setType(scopeType(iterator->GetType()))
                     .setObject(std::move(object))
                     .build();

    String16 name = toProtocolStringWithTypeCheck(
        isolate, iterator->GetFunctionDebugName());
    if (!name.isEmpty()) scope->setName(name);

    if (iterator->HasLocationInfo()) {
      v8::debug::Location start = iterator->GetStartLocation();
      scope->setStartLocation(protocol::Debugger::Location::create()
                                  .setScriptId(scriptId)
                                  .setLineNumber(start.GetLineNumber())
                                  .setColumnNumber(start.GetColumnNumber())
                                  .build());

      v8::debug::Location end = iterator->GetEndLocation();
      scope->setEndLocation(protocol::Debugger::Location::create()
                                .setScriptId(scriptId)
                                .setLineNumber(end.GetLineNumber())
                                .setColumnNumber(end.GetColumnNumber())
                                .build());
    }
    (*scopes)->emplace_back(std::move(scope));
  }
  return Response::Success();
}

protocol::DictionaryValue* getOrCreateObject(protocol::DictionaryValue* object,
                                             const String16& key) {
  protocol::DictionaryValue* value = object->getObject(key);
  if (value) return value;
  std::unique_ptr<protocol::DictionaryValue> newDictionary =
      protocol::DictionaryValue::create();
  value = newDictionary.get();
  object->setObject(key, std::move(newDictionary));
  return value;
}

Response isValidPosition(protocol::Debugger::ScriptPosition* position) {
  if (position->getLineNumber() < 0)
    return Response::ServerError("Position missing 'line' or 'line' < 0.");
  if (position->getColumnNumber() < 0)
    return Response::ServerError("Position missing 'column' or 'column' < 0.");
  return Response::Success();
}

Response isValidRangeOfPositions(std::vector<std::pair<int, int>>& positions) {
  for (size_t i = 1; i < positions.size(); ++i) {
    if (positions[i - 1].first < positions[i].first) continue;
    if (positions[i - 1].first == positions[i].first &&
        positions[i - 1].second < positions[i].second)
      continue;
    return Response::ServerError(
        "Input positions array is not sorted or contains duplicate values.");
  }
  return Response::Success();
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

V8DebuggerAgentImpl::~V8DebuggerAgentImpl() = default;

void V8DebuggerAgentImpl::enableImpl() {
  m_enabled = true;
  m_state->setBoolean(DebuggerAgentState::debuggerEnabled, true);
  m_debugger->enable();

  std::vector<std::unique_ptr<V8DebuggerScript>> compiledScripts =
      m_debugger->getCompiledScripts(m_session->contextGroupId(), this);
  for (auto& script : compiledScripts) {
    didParseSource(std::move(script), true);
  }

  m_breakpointsActive = true;
  m_debugger->setBreakpointsActive(true);

  if (isPaused()) {
    didPause(0, v8::Local<v8::Value>(), std::vector<v8::debug::BreakpointId>(),
             v8::debug::kException, false, false, false);
  }
}

Response V8DebuggerAgentImpl::enable(Maybe<double> maxScriptsCacheSize,
                                     String16* outDebuggerId) {
  m_maxScriptCacheSize = v8::base::saturated_cast<size_t>(
      maxScriptsCacheSize.fromMaybe(std::numeric_limits<double>::max()));
  *outDebuggerId =
      m_debugger->debuggerIdFor(m_session->contextGroupId()).toString();
  if (enabled()) return Response::Success();

  if (!m_inspector->client()->canExecuteScripts(m_session->contextGroupId()))
    return Response::ServerError("Script execution is prohibited");

  enableImpl();
  return Response::Success();
}

Response V8DebuggerAgentImpl::disable() {
  if (!enabled()) return Response::Success();

  m_state->remove(DebuggerAgentState::breakpointsByRegex);
  m_state->remove(DebuggerAgentState::breakpointsByUrl);
  m_state->remove(DebuggerAgentState::breakpointsByScriptHash);
  m_state->remove(DebuggerAgentState::breakpointHints);
  m_state->remove(DebuggerAgentState::instrumentationBreakpoints);

  m_state->setInteger(DebuggerAgentState::pauseOnExceptionsState,
                      v8::debug::NoBreakOnException);
  m_state->setInteger(DebuggerAgentState::asyncCallStackDepth, 0);

  if (m_breakpointsActive) {
    m_debugger->setBreakpointsActive(false);
    m_breakpointsActive = false;
  }
  m_blackboxedPositions.clear();
  m_blackboxPattern.reset();
  resetBlackboxedStateCache();
  m_skipList.clear();
  m_scripts.clear();
  m_cachedScriptIds.clear();
  m_cachedScriptSize = 0;
  for (const auto& it : m_debuggerBreakpointIdToBreakpointId) {
    v8::debug::RemoveBreakpoint(m_isolate, it.first);
  }
  m_breakpointIdToDebuggerBreakpointIds.clear();
  m_debuggerBreakpointIdToBreakpointId.clear();
  m_debugger->setAsyncCallStackDepth(this, 0);
  clearBreakDetails();
  m_skipAllPauses = false;
  m_state->setBoolean(DebuggerAgentState::skipAllPauses, false);
  m_state->remove(DebuggerAgentState::blackboxPattern);
  m_enabled = false;
  m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
  m_debugger->disable();
  return Response::Success();
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
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  if (m_breakpointsActive == active) return Response::Success();
  m_breakpointsActive = active;
  m_debugger->setBreakpointsActive(active);
  if (!active && !m_breakReason.empty()) {
    clearBreakDetails();
    m_debugger->setPauseOnNextCall(false, m_session->contextGroupId());
  }
  return Response::Success();
}

Response V8DebuggerAgentImpl::setSkipAllPauses(bool skip) {
  m_state->setBoolean(DebuggerAgentState::skipAllPauses, skip);
  m_skipAllPauses = skip;
  return Response::Success();
}

static bool matches(V8InspectorImpl* inspector, const V8DebuggerScript& script,
                    BreakpointType type, const String16& selector) {
  switch (type) {
    case BreakpointType::kByUrl:
      return script.sourceURL() == selector;
    case BreakpointType::kByScriptHash:
      return script.hash() == selector;
    case BreakpointType::kByUrlRegex: {
      V8Regex regex(inspector, selector, true);
      return regex.match(script.sourceURL()) != -1;
    }
    case BreakpointType::kByScriptId: {
      return script.scriptId() == selector;
    }
    default:
      return false;
  }
}

Response V8DebuggerAgentImpl::setBreakpointByUrl(
    int lineNumber, Maybe<String16> optionalURL,
    Maybe<String16> optionalURLRegex, Maybe<String16> optionalScriptHash,
    Maybe<int> optionalColumnNumber, Maybe<String16> optionalCondition,
    String16* outBreakpointId,
    std::unique_ptr<protocol::Array<protocol::Debugger::Location>>* locations) {
  *locations = std::make_unique<Array<protocol::Debugger::Location>>();

  int specified = (optionalURL.isJust() ? 1 : 0) +
                  (optionalURLRegex.isJust() ? 1 : 0) +
                  (optionalScriptHash.isJust() ? 1 : 0);
  if (specified != 1) {
    return Response::ServerError(
        "Either url or urlRegex or scriptHash must be specified.");
  }
  int columnNumber = 0;
  if (optionalColumnNumber.isJust()) {
    columnNumber = optionalColumnNumber.fromJust();
    if (columnNumber < 0)
      return Response::ServerError("Incorrect column number");
  }

  BreakpointType type = BreakpointType::kByUrl;
  String16 selector;
  if (optionalURLRegex.isJust()) {
    selector = optionalURLRegex.fromJust();
    type = BreakpointType::kByUrlRegex;
  } else if (optionalURL.isJust()) {
    selector = optionalURL.fromJust();
    type = BreakpointType::kByUrl;
  } else if (optionalScriptHash.isJust()) {
    selector = optionalScriptHash.fromJust();
    type = BreakpointType::kByScriptHash;
  }

  String16 condition = optionalCondition.fromMaybe(String16());
  String16 breakpointId =
      generateBreakpointId(type, selector, lineNumber, columnNumber);
  protocol::DictionaryValue* breakpoints;
  switch (type) {
    case BreakpointType::kByUrlRegex:
      breakpoints =
          getOrCreateObject(m_state, DebuggerAgentState::breakpointsByRegex);
      break;
    case BreakpointType::kByUrl:
      breakpoints = getOrCreateObject(
          getOrCreateObject(m_state, DebuggerAgentState::breakpointsByUrl),
          selector);
      break;
    case BreakpointType::kByScriptHash:
      breakpoints = getOrCreateObject(
          getOrCreateObject(m_state,
                            DebuggerAgentState::breakpointsByScriptHash),
          selector);
      break;
    default:
      UNREACHABLE();
  }
  if (breakpoints->get(breakpointId)) {
    return Response::ServerError(
        "Breakpoint at specified location already exists.");
  }

  String16 hint;
  for (const auto& script : m_scripts) {
    if (!matches(m_inspector, *script.second, type, selector)) continue;
    if (!hint.isEmpty()) {
      adjustBreakpointLocation(*script.second, hint, &lineNumber,
                               &columnNumber);
    }
    std::unique_ptr<protocol::Debugger::Location> location = setBreakpointImpl(
        breakpointId, script.first, condition, lineNumber, columnNumber);
    if (location && type != BreakpointType::kByUrlRegex) {
      hint = breakpointHint(*script.second, lineNumber, columnNumber);
    }
    if (location) (*locations)->emplace_back(std::move(location));
  }
  breakpoints->setString(breakpointId, condition);
  if (!hint.isEmpty()) {
    protocol::DictionaryValue* breakpointHints =
        getOrCreateObject(m_state, DebuggerAgentState::breakpointHints);
    breakpointHints->setString(breakpointId, hint);
  }
  *outBreakpointId = breakpointId;
  return Response::Success();
}

Response V8DebuggerAgentImpl::setBreakpoint(
    std::unique_ptr<protocol::Debugger::Location> location,
    Maybe<String16> optionalCondition, String16* outBreakpointId,
    std::unique_ptr<protocol::Debugger::Location>* actualLocation) {
  String16 breakpointId = generateBreakpointId(
      BreakpointType::kByScriptId, location->getScriptId(),
      location->getLineNumber(), location->getColumnNumber(0));
  if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) !=
      m_breakpointIdToDebuggerBreakpointIds.end()) {
    return Response::ServerError(
        "Breakpoint at specified location already exists.");
  }
  *actualLocation = setBreakpointImpl(breakpointId, location->getScriptId(),
                                      optionalCondition.fromMaybe(String16()),
                                      location->getLineNumber(),
                                      location->getColumnNumber(0));
  if (!*actualLocation)
    return Response::ServerError("Could not resolve breakpoint");
  *outBreakpointId = breakpointId;
  return Response::Success();
}

Response V8DebuggerAgentImpl::setBreakpointOnFunctionCall(
    const String16& functionObjectId, Maybe<String16> optionalCondition,
    String16* outBreakpointId) {
  InjectedScript::ObjectScope scope(m_session, functionObjectId);
  Response response = scope.initialize();
  if (!response.IsSuccess()) return response;
  if (!scope.object()->IsFunction()) {
    return Response::ServerError("Could not find function with given id");
  }
  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(scope.object());
  String16 breakpointId =
      generateBreakpointId(BreakpointType::kBreakpointAtEntry, function);
  if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) !=
      m_breakpointIdToDebuggerBreakpointIds.end()) {
    return Response::ServerError(
        "Breakpoint at specified location already exists.");
  }
  v8::Local<v8::String> condition =
      toV8String(m_isolate, optionalCondition.fromMaybe(String16()));
  setBreakpointImpl(breakpointId, function, condition);
  *outBreakpointId = breakpointId;
  return Response::Success();
}

Response V8DebuggerAgentImpl::setInstrumentationBreakpoint(
    const String16& instrumentation, String16* outBreakpointId) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  String16 breakpointId = generateInstrumentationBreakpointId(instrumentation);
  protocol::DictionaryValue* breakpoints = getOrCreateObject(
      m_state, DebuggerAgentState::instrumentationBreakpoints);
  if (breakpoints->get(breakpointId)) {
    return Response::ServerError(
        "Instrumentation breakpoint is already enabled.");
  }
  breakpoints->setBoolean(breakpointId, true);
  *outBreakpointId = breakpointId;
  return Response::Success();
}

Response V8DebuggerAgentImpl::removeBreakpoint(const String16& breakpointId) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  BreakpointType type;
  String16 selector;
  if (!parseBreakpointId(breakpointId, &type, &selector)) {
    return Response::Success();
  }
  protocol::DictionaryValue* breakpoints = nullptr;
  switch (type) {
    case BreakpointType::kByUrl: {
      protocol::DictionaryValue* breakpointsByUrl =
          m_state->getObject(DebuggerAgentState::breakpointsByUrl);
      if (breakpointsByUrl) {
        breakpoints = breakpointsByUrl->getObject(selector);
      }
    } break;
    case BreakpointType::kByScriptHash: {
      protocol::DictionaryValue* breakpointsByScriptHash =
          m_state->getObject(DebuggerAgentState::breakpointsByScriptHash);
      if (breakpointsByScriptHash) {
        breakpoints = breakpointsByScriptHash->getObject(selector);
      }
    } break;
    case BreakpointType::kByUrlRegex:
      breakpoints = m_state->getObject(DebuggerAgentState::breakpointsByRegex);
      break;
    case BreakpointType::kInstrumentationBreakpoint:
      breakpoints =
          m_state->getObject(DebuggerAgentState::instrumentationBreakpoints);
      break;
    default:
      break;
  }
  if (breakpoints) breakpoints->remove(breakpointId);
  protocol::DictionaryValue* breakpointHints =
      m_state->getObject(DebuggerAgentState::breakpointHints);
  if (breakpointHints) breakpointHints->remove(breakpointId);

  // Get a list of scripts to remove breakpoints.
  // TODO(duongn): we can do better here if from breakpoint id we can tell it is
  // not Wasm breakpoint.
  std::vector<V8DebuggerScript*> scripts;
  for (const auto& scriptIter : m_scripts) {
    if (!matches(m_inspector, *scriptIter.second, type, selector)) continue;
    V8DebuggerScript* script = scriptIter.second.get();
    scripts.push_back(script);
  }
  removeBreakpointImpl(breakpointId, scripts);

  return Response::Success();
}

void V8DebuggerAgentImpl::removeBreakpointImpl(
    const String16& breakpointId,
    const std::vector<V8DebuggerScript*>& scripts) {
  DCHECK(enabled());
  BreakpointIdToDebuggerBreakpointIdsMap::iterator
      debuggerBreakpointIdsIterator =
          m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
  if (debuggerBreakpointIdsIterator ==
      m_breakpointIdToDebuggerBreakpointIds.end()) {
    return;
  }
  for (const auto& id : debuggerBreakpointIdsIterator->second) {
    for (auto& script : scripts) {
      script->removeWasmBreakpoint(id);
    }
    v8::debug::RemoveBreakpoint(m_isolate, id);
    m_debuggerBreakpointIdToBreakpointId.erase(id);
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
    return Response::ServerError(
        "start.lineNumber and start.columnNumber should be >= 0");

  v8::debug::Location v8Start(start->getLineNumber(),
                              start->getColumnNumber(0));
  v8::debug::Location v8End;
  if (end.isJust()) {
    if (end.fromJust()->getScriptId() != scriptId)
      return Response::ServerError(
          "Locations should contain the same scriptId");
    int line = end.fromJust()->getLineNumber();
    int column = end.fromJust()->getColumnNumber(0);
    if (line < 0 || column < 0)
      return Response::ServerError(
          "end.lineNumber and end.columnNumber should be >= 0");
    v8End = v8::debug::Location(line, column);
  }
  auto it = m_scripts.find(scriptId);
  if (it == m_scripts.end()) return Response::ServerError("Script not found");
  std::vector<v8::debug::BreakLocation> v8Locations;
  {
    v8::HandleScope handleScope(m_isolate);
    int contextId = it->second->executionContextId();
    InspectedContext* inspected = m_inspector->getContext(contextId);
    if (!inspected) {
      return Response::ServerError("Cannot retrive script context");
    }
    v8::Context::Scope contextScope(inspected->context());
    v8::MicrotasksScope microtasks(m_isolate,
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::TryCatch tryCatch(m_isolate);
    it->second->getPossibleBreakpoints(
        v8Start, v8End, restrictToFunction.fromMaybe(false), &v8Locations);
  }

  *locations =
      std::make_unique<protocol::Array<protocol::Debugger::BreakLocation>>();

  // TODO(1106269): Return an error instead of capping the number of
  // breakpoints.
  const size_t numBreakpointsToSend =
      std::min(v8Locations.size(), kMaxNumBreakpoints);
  for (size_t i = 0; i < numBreakpointsToSend; ++i) {
    std::unique_ptr<protocol::Debugger::BreakLocation> breakLocation =
        protocol::Debugger::BreakLocation::create()
            .setScriptId(scriptId)
            .setLineNumber(v8Locations[i].GetLineNumber())
            .setColumnNumber(v8Locations[i].GetColumnNumber())
            .build();
    if (v8Locations[i].type() != v8::debug::kCommonBreakLocation) {
      breakLocation->setType(breakLocationType(v8Locations[i].type()));
    }
    (*locations)->emplace_back(std::move(breakLocation));
  }
  return Response::Success();
}

Response V8DebuggerAgentImpl::continueToLocation(
    std::unique_ptr<protocol::Debugger::Location> location,
    Maybe<String16> targetCallFrames) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  ScriptsMap::iterator it = m_scripts.find(location->getScriptId());
  if (it == m_scripts.end()) {
    return Response::ServerError("Cannot continue to specified location");
  }
  V8DebuggerScript* script = it->second.get();
  int contextId = script->executionContextId();
  InspectedContext* inspected = m_inspector->getContext(contextId);
  if (!inspected)
    return Response::ServerError("Cannot continue to specified location");
  v8::HandleScope handleScope(m_isolate);
  v8::Context::Scope contextScope(inspected->context());
  return m_debugger->continueToLocation(
      m_session->contextGroupId(), script, std::move(location),
      targetCallFrames.fromMaybe(
          protocol::Debugger::ContinueToLocation::TargetCallFramesEnum::Any));
}

Response V8DebuggerAgentImpl::getStackTrace(
    std::unique_ptr<protocol::Runtime::StackTraceId> inStackTraceId,
    std::unique_ptr<protocol::Runtime::StackTrace>* outStackTrace) {
  bool isOk = false;
  int64_t id = inStackTraceId->getId().toInteger64(&isOk);
  if (!isOk) return Response::ServerError("Invalid stack trace id");

  V8DebuggerId debuggerId;
  if (inStackTraceId->hasDebuggerId()) {
    debuggerId = V8DebuggerId(inStackTraceId->getDebuggerId(String16()));
  } else {
    debuggerId = m_debugger->debuggerIdFor(m_session->contextGroupId());
  }
  if (!debuggerId.isValid())
    return Response::ServerError("Invalid stack trace id");

  V8StackTraceId v8StackTraceId(id, debuggerId.pair());
  if (v8StackTraceId.IsInvalid())
    return Response::ServerError("Invalid stack trace id");
  auto stack =
      m_debugger->stackTraceFor(m_session->contextGroupId(), v8StackTraceId);
  if (!stack) {
    return Response::ServerError("Stack trace with given id is not found");
  }
  *outStackTrace = stack->buildInspectorObject(
      m_debugger, m_debugger->maxAsyncCallChainDepth());
  return Response::Success();
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

bool V8DebuggerAgentImpl::shouldBeSkipped(const String16& scriptId, int line,
                                          int column) {
  if (m_skipList.empty()) return false;

  auto it = m_skipList.find(scriptId);
  if (it == m_skipList.end()) return false;

  const std::vector<std::pair<int, int>>& ranges = it->second;
  DCHECK(!ranges.empty());
  const std::pair<int, int> location = std::make_pair(line, column);
  auto itLowerBound = std::lower_bound(ranges.begin(), ranges.end(), location,
                                       positionComparator);

  bool shouldSkip = false;
  if (itLowerBound != ranges.end()) {
    // Skip lists are defined as pairs of locations that specify the
    // start and the end of ranges to skip: [ranges[0], ranges[1], ..], where
    // locations in [ranges[0], ranges[1]) should be skipped, i.e.
    // [(lineStart, columnStart), (lineEnd, columnEnd)).
    const bool isSameAsLowerBound = location == *itLowerBound;
    const bool isUnevenIndex = (itLowerBound - ranges.begin()) % 2;
    shouldSkip = isSameAsLowerBound ^ isUnevenIndex;
  }

  return shouldSkip;
}

bool V8DebuggerAgentImpl::acceptsPause(bool isOOMBreak) const {
  return enabled() && (isOOMBreak || !m_skipAllPauses);
}

std::unique_ptr<protocol::Debugger::Location>
V8DebuggerAgentImpl::setBreakpointImpl(const String16& breakpointId,
                                       const String16& scriptId,
                                       const String16& condition,
                                       int lineNumber, int columnNumber) {
  v8::HandleScope handles(m_isolate);
  DCHECK(enabled());

  ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
  if (scriptIterator == m_scripts.end()) return nullptr;
  V8DebuggerScript* script = scriptIterator->second.get();
  if (lineNumber < script->startLine() || script->endLine() < lineNumber) {
    return nullptr;
  }

  v8::debug::BreakpointId debuggerBreakpointId;
  v8::debug::Location location(lineNumber, columnNumber);
  int contextId = script->executionContextId();
  InspectedContext* inspected = m_inspector->getContext(contextId);
  if (!inspected) return nullptr;

  {
    v8::Context::Scope contextScope(inspected->context());
    if (!script->setBreakpoint(condition, &location, &debuggerBreakpointId)) {
      return nullptr;
    }
  }

  m_debuggerBreakpointIdToBreakpointId[debuggerBreakpointId] = breakpointId;
  m_breakpointIdToDebuggerBreakpointIds[breakpointId].push_back(
      debuggerBreakpointId);

  return protocol::Debugger::Location::create()
      .setScriptId(scriptId)
      .setLineNumber(location.GetLineNumber())
      .setColumnNumber(location.GetColumnNumber())
      .build();
}

void V8DebuggerAgentImpl::setBreakpointImpl(const String16& breakpointId,
                                            v8::Local<v8::Function> function,
                                            v8::Local<v8::String> condition) {
  v8::debug::BreakpointId debuggerBreakpointId;
  if (!v8::debug::SetFunctionBreakpoint(function, condition,
                                        &debuggerBreakpointId)) {
    return;
  }
  m_debuggerBreakpointIdToBreakpointId[debuggerBreakpointId] = breakpointId;
  m_breakpointIdToDebuggerBreakpointIds[breakpointId].push_back(
      debuggerBreakpointId);
}

Response V8DebuggerAgentImpl::searchInContent(
    const String16& scriptId, const String16& query,
    Maybe<bool> optionalCaseSensitive, Maybe<bool> optionalIsRegex,
    std::unique_ptr<Array<protocol::Debugger::SearchMatch>>* results) {
  v8::HandleScope handles(m_isolate);
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end())
    return Response::ServerError("No script for id: " + scriptId.utf8());

  *results = std::make_unique<protocol::Array<protocol::Debugger::SearchMatch>>(
      searchInTextByLinesImpl(m_session, it->second->source(0), query,
                              optionalCaseSensitive.fromMaybe(false),
                              optionalIsRegex.fromMaybe(false)));
  return Response::Success();
}

Response V8DebuggerAgentImpl::setScriptSource(
    const String16& scriptId, const String16& newContent, Maybe<bool> dryRun,
    Maybe<protocol::Array<protocol::Debugger::CallFrame>>* newCallFrames,
    Maybe<bool>* stackChanged,
    Maybe<protocol::Runtime::StackTrace>* asyncStackTrace,
    Maybe<protocol::Runtime::StackTraceId>* asyncStackTraceId,
    Maybe<protocol::Runtime::ExceptionDetails>* optOutCompileError) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);

  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end()) {
    return Response::ServerError("No script with given id found");
  }
  int contextId = it->second->executionContextId();
  InspectedContext* inspected = m_inspector->getContext(contextId);
  if (!inspected) {
    return Response::InternalError();
  }
  v8::HandleScope handleScope(m_isolate);
  v8::Local<v8::Context> context = inspected->context();
  v8::Context::Scope contextScope(context);

  v8::debug::LiveEditResult result;
  it->second->setSource(newContent, dryRun.fromMaybe(false), &result);
  if (result.status != v8::debug::LiveEditResult::OK) {
    *optOutCompileError =
        protocol::Runtime::ExceptionDetails::create()
            .setExceptionId(m_inspector->nextExceptionId())
            .setText(toProtocolString(m_isolate, result.message))
            .setLineNumber(result.line_number != -1 ? result.line_number - 1
                                                    : 0)
            .setColumnNumber(result.column_number != -1 ? result.column_number
                                                        : 0)
            .build();
    return Response::Success();
  } else {
    *stackChanged = result.stack_changed;
  }
  std::unique_ptr<Array<CallFrame>> callFrames;
  Response response = currentCallFrames(&callFrames);
  if (!response.IsSuccess()) return response;
  *newCallFrames = std::move(callFrames);
  *asyncStackTrace = currentAsyncStackTrace();
  *asyncStackTraceId = currentExternalStackTrace();
  return Response::Success();
}

Response V8DebuggerAgentImpl::restartFrame(
    const String16& callFrameId,
    std::unique_ptr<Array<CallFrame>>* newCallFrames,
    Maybe<protocol::Runtime::StackTrace>* asyncStackTrace,
    Maybe<protocol::Runtime::StackTraceId>* asyncStackTraceId) {
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  InjectedScript::CallFrameScope scope(m_session, callFrameId);
  Response response = scope.initialize();
  if (!response.IsSuccess()) return response;
  int frameOrdinal = static_cast<int>(scope.frameOrdinal());
  auto it = v8::debug::StackTraceIterator::Create(m_isolate, frameOrdinal);
  if (it->Done()) {
    return Response::ServerError("Could not find call frame with given id");
  }
  if (!it->Restart()) {
    return Response::InternalError();
  }
  response = currentCallFrames(newCallFrames);
  if (!response.IsSuccess()) return response;
  *asyncStackTrace = currentAsyncStackTrace();
  *asyncStackTraceId = currentExternalStackTrace();
  return Response::Success();
}

Response V8DebuggerAgentImpl::getScriptSource(
    const String16& scriptId, String16* scriptSource,
    Maybe<protocol::Binary>* bytecode) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end())
    return Response::ServerError("No script for id: " + scriptId.utf8());
  *scriptSource = it->second->source(0);
  v8::MemorySpan<const uint8_t> span;
  if (it->second->wasmBytecode().To(&span)) {
    if (span.size() > kWasmBytecodeMaxLength) {
      return Response::ServerError(kWasmBytecodeExceedsTransferLimit);
    }
    *bytecode = protocol::Binary::fromSpan(span.data(), span.size());
  }
  return Response::Success();
}

Response V8DebuggerAgentImpl::getWasmBytecode(const String16& scriptId,
                                              protocol::Binary* bytecode) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  ScriptsMap::iterator it = m_scripts.find(scriptId);
  if (it == m_scripts.end())
    return Response::ServerError("No script for id: " + scriptId.utf8());
  v8::MemorySpan<const uint8_t> span;
  if (!it->second->wasmBytecode().To(&span))
    return Response::ServerError("Script with id " + scriptId.utf8() +
                                 " is not WebAssembly");
  if (span.size() > kWasmBytecodeMaxLength) {
    return Response::ServerError(kWasmBytecodeExceedsTransferLimit);
  }
  *bytecode = protocol::Binary::fromSpan(span.data(), span.size());
  return Response::Success();
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
  if (isPaused() || !acceptsPause(false) || !m_breakpointsActive) return;
  if (m_breakReason.empty()) {
    m_debugger->setPauseOnNextCall(true, m_session->contextGroupId());
  }
  pushBreakDetails(breakReason, std::move(data));
}

void V8DebuggerAgentImpl::cancelPauseOnNextStatement() {
  if (isPaused() || !acceptsPause(false) || !m_breakpointsActive) return;
  if (m_breakReason.size() == 1) {
    m_debugger->setPauseOnNextCall(false, m_session->contextGroupId());
  }
  popBreakDetails();
}

Response V8DebuggerAgentImpl::pause() {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  if (isPaused()) return Response::Success();
  if (m_debugger->canBreakProgram()) {
    m_debugger->interruptAndBreak(m_session->contextGroupId());
  } else {
    if (m_breakReason.empty()) {
      m_debugger->setPauseOnNextCall(true, m_session->contextGroupId());
    }
    pushBreakDetails(protocol::Debugger::Paused::ReasonEnum::Other, nullptr);
  }
  return Response::Success();
}

Response V8DebuggerAgentImpl::resume(Maybe<bool> terminateOnResume) {
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->continueProgram(m_session->contextGroupId(),
                              terminateOnResume.fromMaybe(false));
  return Response::Success();
}

Response V8DebuggerAgentImpl::stepOver(
    Maybe<protocol::Array<protocol::Debugger::LocationRange>> inSkipList) {
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);

  if (inSkipList.isJust()) {
    const Response res = processSkipList(inSkipList.fromJust());
    if (res.IsError()) return res;
  } else {
    m_skipList.clear();
  }

  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->stepOverStatement(m_session->contextGroupId());
  return Response::Success();
}

Response V8DebuggerAgentImpl::stepInto(
    Maybe<bool> inBreakOnAsyncCall,
    Maybe<protocol::Array<protocol::Debugger::LocationRange>> inSkipList) {
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);

  if (inSkipList.isJust()) {
    const Response res = processSkipList(inSkipList.fromJust());
    if (res.IsError()) return res;
  } else {
    m_skipList.clear();
  }

  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->stepIntoStatement(m_session->contextGroupId(),
                                inBreakOnAsyncCall.fromMaybe(false));
  return Response::Success();
}

Response V8DebuggerAgentImpl::stepOut() {
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  m_session->releaseObjectGroup(kBacktraceObjectGroup);
  m_debugger->stepOutOfFunction(m_session->contextGroupId());
  return Response::Success();
}

Response V8DebuggerAgentImpl::pauseOnAsyncCall(
    std::unique_ptr<protocol::Runtime::StackTraceId> inParentStackTraceId) {
  // Deprecated, just return OK.
  return Response::Success();
}

Response V8DebuggerAgentImpl::setPauseOnExceptions(
    const String16& stringPauseState) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  v8::debug::ExceptionBreakState pauseState;
  if (stringPauseState == "none") {
    pauseState = v8::debug::NoBreakOnException;
  } else if (stringPauseState == "all") {
    pauseState = v8::debug::BreakOnAnyException;
  } else if (stringPauseState == "uncaught") {
    pauseState = v8::debug::BreakOnUncaughtException;
  } else {
    return Response::ServerError("Unknown pause on exceptions mode: " +
                                 stringPauseState.utf8());
  }
  setPauseOnExceptionsImpl(pauseState);
  return Response::Success();
}

void V8DebuggerAgentImpl::setPauseOnExceptionsImpl(int pauseState) {
  // TODO(dgozman): this changes the global state and forces all context groups
  // to pause. We should make this flag be per-context-group.
  m_debugger->setPauseOnExceptionsState(
      static_cast<v8::debug::ExceptionBreakState>(pauseState));
  m_state->setInteger(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

Response V8DebuggerAgentImpl::evaluateOnCallFrame(
    const String16& callFrameId, const String16& expression,
    Maybe<String16> objectGroup, Maybe<bool> includeCommandLineAPI,
    Maybe<bool> silent, Maybe<bool> returnByValue, Maybe<bool> generatePreview,
    Maybe<bool> throwOnSideEffect, Maybe<double> timeout,
    std::unique_ptr<RemoteObject>* result,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  InjectedScript::CallFrameScope scope(m_session, callFrameId);
  Response response = scope.initialize();
  if (!response.IsSuccess()) return response;
  if (includeCommandLineAPI.fromMaybe(false)) scope.installCommandLineAPI();
  if (silent.fromMaybe(false)) scope.ignoreExceptionsAndMuteConsole();

  int frameOrdinal = static_cast<int>(scope.frameOrdinal());
  auto it = v8::debug::StackTraceIterator::Create(m_isolate, frameOrdinal);
  if (it->Done()) {
    return Response::ServerError("Could not find call frame with given id");
  }

  v8::MaybeLocal<v8::Value> maybeResultValue;
  {
    V8InspectorImpl::EvaluateScope evaluateScope(scope);
    if (timeout.isJust()) {
      response = evaluateScope.setTimeout(timeout.fromJust() / 1000.0);
      if (!response.IsSuccess()) return response;
    }
    maybeResultValue = it->Evaluate(toV8String(m_isolate, expression),
                                    throwOnSideEffect.fromMaybe(false));
  }
  // Re-initialize after running client's code, as it could have destroyed
  // context or session.
  response = scope.initialize();
  if (!response.IsSuccess()) return response;
  WrapMode mode = generatePreview.fromMaybe(false) ? WrapMode::kWithPreview
                                                   : WrapMode::kNoPreview;
  if (returnByValue.fromMaybe(false)) mode = WrapMode::kForceValue;
  return scope.injectedScript()->wrapEvaluateResult(
      maybeResultValue, scope.tryCatch(), objectGroup.fromMaybe(""), mode,
      result, exceptionDetails);
}

Response V8DebuggerAgentImpl::setVariableValue(
    int scopeNumber, const String16& variableName,
    std::unique_ptr<protocol::Runtime::CallArgument> newValueArgument,
    const String16& callFrameId) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  InjectedScript::CallFrameScope scope(m_session, callFrameId);
  Response response = scope.initialize();
  if (!response.IsSuccess()) return response;
  v8::Local<v8::Value> newValue;
  response = scope.injectedScript()->resolveCallArgument(newValueArgument.get(),
                                                         &newValue);
  if (!response.IsSuccess()) return response;

  int frameOrdinal = static_cast<int>(scope.frameOrdinal());
  auto it = v8::debug::StackTraceIterator::Create(m_isolate, frameOrdinal);
  if (it->Done()) {
    return Response::ServerError("Could not find call frame with given id");
  }
  auto scopeIterator = it->GetScopeIterator();
  while (!scopeIterator->Done() && scopeNumber > 0) {
    --scopeNumber;
    scopeIterator->Advance();
  }
  if (scopeNumber != 0) {
    return Response::ServerError("Could not find scope with given number");
  }

  if (!scopeIterator->SetVariableValue(toV8String(m_isolate, variableName),
                                       newValue) ||
      scope.tryCatch().HasCaught()) {
    return Response::InternalError();
  }
  return Response::Success();
}

Response V8DebuggerAgentImpl::setReturnValue(
    std::unique_ptr<protocol::Runtime::CallArgument> protocolNewValue) {
  if (!enabled()) return Response::ServerError(kDebuggerNotEnabled);
  if (!isPaused()) return Response::ServerError(kDebuggerNotPaused);
  v8::HandleScope handleScope(m_isolate);
  auto iterator = v8::debug::StackTraceIterator::Create(m_isolate);
  if (iterator->Done()) {
    return Response::ServerError("Could not find top call frame");
  }
  if (iterator->GetReturnValue().IsEmpty()) {
    return Response::ServerError(
        "Could not update return value at non-return position");
  }
  InjectedScript::ContextScope scope(m_session, iterator->GetContextId());
  Response response = scope.initialize();
  if (!response.IsSuccess()) return response;
  v8::Local<v8::Value> newValue;
  response = scope.injectedScript()->resolveCallArgument(protocolNewValue.get(),
                                                         &newValue);
  if (!response.IsSuccess()) return response;
  v8::debug::SetReturnValue(m_isolate, newValue);
  return Response::Success();
}

Response V8DebuggerAgentImpl::setAsyncCallStackDepth(int depth) {
  if (!enabled() && !m_session->runtimeAgent()->enabled()) {
    return Response::ServerError(kDebuggerNotEnabled);
  }
  m_state->setInteger(DebuggerAgentState::asyncCallStackDepth, depth);
  m_debugger->setAsyncCallStackDepth(this, depth);
  return Response::Success();
}

Response V8DebuggerAgentImpl::setBlackboxPatterns(
    std::unique_ptr<protocol::Array<String16>> patterns) {
  if (patterns->empty()) {
    m_blackboxPattern = nullptr;
    resetBlackboxedStateCache();
    m_state->remove(DebuggerAgentState::blackboxPattern);
    return Response::Success();
  }

  String16Builder patternBuilder;
  patternBuilder.append('(');
  for (size_t i = 0; i < patterns->size() - 1; ++i) {
    patternBuilder.append((*patterns)[i]);
    patternBuilder.append("|");
  }
  patternBuilder.append(patterns->back());
  patternBuilder.append(')');
  String16 pattern = patternBuilder.toString();
  Response response = setBlackboxPattern(pattern);
  if (!response.IsSuccess()) return response;
  resetBlackboxedStateCache();
  m_state->setString(DebuggerAgentState::blackboxPattern, pattern);
  return Response::Success();
}

Response V8DebuggerAgentImpl::setBlackboxPattern(const String16& pattern) {
  std::unique_ptr<V8Regex> regex(new V8Regex(
      m_inspector, pattern, true /** caseSensitive */, false /** multiline */));
  if (!regex->isValid())
    return Response::ServerError("Pattern parser error: " +
                                 regex->errorMessage().utf8());
  m_blackboxPattern = std::move(regex);
  return Response::Success();
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
    return Response::ServerError("No script with passed id.");

  if (inPositions->empty()) {
    m_blackboxedPositions.erase(scriptId);
    it->second->resetBlackboxedStateCache();
    return Response::Success();
  }

  std::vector<std::pair<int, int>> positions;
  positions.reserve(inPositions->size());
  for (const std::unique_ptr<protocol::Debugger::ScriptPosition>& position :
       *inPositions) {
    Response res = isValidPosition(position.get());
    if (res.IsError()) return res;

    positions.push_back(
        std::make_pair(position->getLineNumber(), position->getColumnNumber()));
  }
  Response res = isValidRangeOfPositions(positions);
  if (res.IsError()) return res;

  m_blackboxedPositions[scriptId] = positions;
  it->second->resetBlackboxedStateCache();
  return Response::Success();
}

Response V8DebuggerAgentImpl::currentCallFrames(
    std::unique_ptr<Array<CallFrame>>* result) {
  if (!isPaused()) {
    *result = std::make_unique<Array<CallFrame>>();
    return Response::Success();
  }
  v8::HandleScope handles(m_isolate);
  *result = std::make_unique<Array<CallFrame>>();
  auto iterator = v8::debug::StackTraceIterator::Create(m_isolate);
  int frameOrdinal = 0;
  for (; !iterator->Done(); iterator->Advance(), frameOrdinal++) {
    int contextId = iterator->GetContextId();
    InjectedScript* injectedScript = nullptr;
    if (contextId) m_session->findInjectedScript(contextId, injectedScript);
    String16 callFrameId = RemoteCallFrameId::serialize(
        m_inspector->isolateId(), contextId, frameOrdinal);

    v8::debug::Location loc = iterator->GetSourceLocation();

    std::unique_ptr<Array<Scope>> scopes;
    auto scopeIterator = iterator->GetScopeIterator();
    Response res =
        buildScopes(m_isolate, scopeIterator.get(), injectedScript, &scopes);
    if (!res.IsSuccess()) return res;

    std::unique_ptr<RemoteObject> protocolReceiver;
    if (injectedScript) {
      v8::Local<v8::Value> receiver;
      if (iterator->GetReceiver().ToLocal(&receiver)) {
        res =
            injectedScript->wrapObject(receiver, kBacktraceObjectGroup,
                                       WrapMode::kNoPreview, &protocolReceiver);
        if (!res.IsSuccess()) return res;
      }
    }
    if (!protocolReceiver) {
      protocolReceiver = RemoteObject::create()
                             .setType(RemoteObject::TypeEnum::Undefined)
                             .build();
    }

    v8::Local<v8::debug::Script> script = iterator->GetScript();
    DCHECK(!script.IsEmpty());
    std::unique_ptr<protocol::Debugger::Location> location =
        protocol::Debugger::Location::create()
            .setScriptId(String16::fromInteger(script->Id()))
            .setLineNumber(loc.GetLineNumber())
            .setColumnNumber(loc.GetColumnNumber())
            .build();
    String16 scriptId = String16::fromInteger(script->Id());
    ScriptsMap::iterator scriptIterator =
        m_scripts.find(location->getScriptId());
    String16 url;
    if (scriptIterator != m_scripts.end()) {
      url = scriptIterator->second->sourceURL();
    }

    auto frame = CallFrame::create()
                     .setCallFrameId(callFrameId)
                     .setFunctionName(toProtocolString(
                         m_isolate, iterator->GetFunctionDebugName()))
                     .setLocation(std::move(location))
                     .setUrl(url)
                     .setScopeChain(std::move(scopes))
                     .setThis(std::move(protocolReceiver))
                     .build();

    v8::Local<v8::Function> func = iterator->GetFunction();
    if (!func.IsEmpty()) {
      frame->setFunctionLocation(
          protocol::Debugger::Location::create()
              .setScriptId(String16::fromInteger(func->ScriptId()))
              .setLineNumber(func->GetScriptLineNumber())
              .setColumnNumber(func->GetScriptColumnNumber())
              .build());
    }

    v8::Local<v8::Value> returnValue = iterator->GetReturnValue();
    if (!returnValue.IsEmpty() && injectedScript) {
      std::unique_ptr<RemoteObject> value;
      res = injectedScript->wrapObject(returnValue, kBacktraceObjectGroup,
                                       WrapMode::kNoPreview, &value);
      if (!res.IsSuccess()) return res;
      frame->setReturnValue(std::move(value));
    }
    (*result)->emplace_back(std::move(frame));
  }
  return Response::Success();
}

std::unique_ptr<protocol::Runtime::StackTrace>
V8DebuggerAgentImpl::currentAsyncStackTrace() {
  std::shared_ptr<AsyncStackTrace> asyncParent =
      m_debugger->currentAsyncParent();
  if (!asyncParent) return nullptr;
  return asyncParent->buildInspectorObject(
      m_debugger, m_debugger->maxAsyncCallChainDepth() - 1);
}

std::unique_ptr<protocol::Runtime::StackTraceId>
V8DebuggerAgentImpl::currentExternalStackTrace() {
  V8StackTraceId externalParent = m_debugger->currentExternalParent();
  if (externalParent.IsInvalid()) return nullptr;
  return protocol::Runtime::StackTraceId::create()
      .setId(stackTraceIdToString(externalParent.id))
      .setDebuggerId(V8DebuggerId(externalParent.debugger_id).toString())
      .build();
}

bool V8DebuggerAgentImpl::isPaused() const {
  return m_debugger->isPausedInContextGroup(m_session->contextGroupId());
}

static String16 getScriptLanguage(const V8DebuggerScript& script) {
  switch (script.getLanguage()) {
    case V8DebuggerScript::Language::WebAssembly:
      return protocol::Debugger::ScriptLanguageEnum::WebAssembly;
    case V8DebuggerScript::Language::JavaScript:
      return protocol::Debugger::ScriptLanguageEnum::JavaScript;
  }
}

static const char* getDebugSymbolTypeName(
    v8::debug::WasmScript::DebugSymbolsType type) {
  switch (type) {
    case v8::debug::WasmScript::DebugSymbolsType::None:
      return v8_inspector::protocol::Debugger::DebugSymbols::TypeEnum::None;
    case v8::debug::WasmScript::DebugSymbolsType::SourceMap:
      return v8_inspector::protocol::Debugger::DebugSymbols::TypeEnum::
          SourceMap;
    case v8::debug::WasmScript::DebugSymbolsType::EmbeddedDWARF:
      return v8_inspector::protocol::Debugger::DebugSymbols::TypeEnum::
          EmbeddedDWARF;
    case v8::debug::WasmScript::DebugSymbolsType::ExternalDWARF:
      return v8_inspector::protocol::Debugger::DebugSymbols::TypeEnum::
          ExternalDWARF;
  }
}

static std::unique_ptr<protocol::Debugger::DebugSymbols> getDebugSymbols(
    const V8DebuggerScript& script) {
  v8::debug::WasmScript::DebugSymbolsType type;
  if (!script.getDebugSymbolsType().To(&type)) return {};

  std::unique_ptr<protocol::Debugger::DebugSymbols> debugSymbols =
      v8_inspector::protocol::Debugger::DebugSymbols::create()
          .setType(getDebugSymbolTypeName(type))
          .build();
  String16 externalUrl;
  if (script.getExternalDebugSymbolsURL().To(&externalUrl)) {
    debugSymbols->setExternalURL(externalUrl);
  }
  return debugSymbols;
}

void V8DebuggerAgentImpl::didParseSource(
    std::unique_ptr<V8DebuggerScript> script, bool success) {
  v8::HandleScope handles(m_isolate);
  if (!success) {
    DCHECK(!script->isSourceLoadedLazily());
    String16 scriptSource = script->source(0);
    script->setSourceURL(findSourceURL(scriptSource, false));
    script->setSourceMappingURL(findSourceMapURL(scriptSource, false));
  }

  int contextId = script->executionContextId();
  int contextGroupId = m_inspector->contextGroupId(contextId);
  InspectedContext* inspected =
      m_inspector->getContext(contextGroupId, contextId);
  std::unique_ptr<protocol::DictionaryValue> executionContextAuxData;
  if (inspected) {
    // Script reused between different groups/sessions can have a stale
    // execution context id.
    const String16& aux = inspected->auxData();
    std::vector<uint8_t> cbor;
    v8_crdtp::json::ConvertJSONToCBOR(
        v8_crdtp::span<uint16_t>(aux.characters16(), aux.length()), &cbor);
    executionContextAuxData = protocol::DictionaryValue::cast(
        protocol::Value::parseBinary(cbor.data(), cbor.size()));
  }
  bool isLiveEdit = script->isLiveEdit();
  bool hasSourceURLComment = script->hasSourceURLComment();
  bool isModule = script->isModule();
  String16 scriptId = script->scriptId();
  String16 scriptURL = script->sourceURL();
  String16 embedderName = script->embedderName();
  String16 scriptLanguage = getScriptLanguage(*script);
  Maybe<int> codeOffset;
  if (script->getLanguage() == V8DebuggerScript::Language::WebAssembly)
    codeOffset = script->codeOffset();
  std::unique_ptr<protocol::Debugger::DebugSymbols> debugSymbols =
      getDebugSymbols(*script);

  m_scripts[scriptId] = std::move(script);
  // Release the strong reference to get notified when debugger is the only
  // one that holds the script. Has to be done after script added to m_scripts.
  m_scripts[scriptId]->MakeWeak();

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
  const bool* hasSourceURLParam =
      hasSourceURLComment ? &hasSourceURLComment : nullptr;
  const bool* isModuleParam = isModule ? &isModule : nullptr;
  std::unique_ptr<V8StackTraceImpl> stack =
      V8StackTraceImpl::capture(m_inspector->debugger(), contextGroupId, 1);
  std::unique_ptr<protocol::Runtime::StackTrace> stackTrace =
      stack && !stack->isEmpty()
          ? stack->buildInspectorObjectImpl(m_debugger, 0)
          : nullptr;

  if (!success) {
    m_frontend.scriptFailedToParse(
        scriptId, scriptURL, scriptRef->startLine(), scriptRef->startColumn(),
        scriptRef->endLine(), scriptRef->endColumn(), contextId,
        scriptRef->hash(), std::move(executionContextAuxDataParam),
        std::move(sourceMapURLParam), hasSourceURLParam, isModuleParam,
        scriptRef->length(), std::move(stackTrace), std::move(codeOffset),
        std::move(scriptLanguage), embedderName);
    return;
  }

  if (scriptRef->isSourceLoadedLazily()) {
    m_frontend.scriptParsed(
        scriptId, scriptURL, 0, 0, 0, 0, contextId, scriptRef->hash(),
        std::move(executionContextAuxDataParam), isLiveEditParam,
        std::move(sourceMapURLParam), hasSourceURLParam, isModuleParam, 0,
        std::move(stackTrace), std::move(codeOffset), std::move(scriptLanguage),
        std::move(debugSymbols), embedderName);
  } else {
    m_frontend.scriptParsed(
        scriptId, scriptURL, scriptRef->startLine(), scriptRef->startColumn(),
        scriptRef->endLine(), scriptRef->endColumn(), contextId,
        scriptRef->hash(), std::move(executionContextAuxDataParam),
        isLiveEditParam, std::move(sourceMapURLParam), hasSourceURLParam,
        isModuleParam, scriptRef->length(), std::move(stackTrace),
        std::move(codeOffset), std::move(scriptLanguage),
        std::move(debugSymbols), embedderName);
  }

  std::vector<protocol::DictionaryValue*> potentialBreakpoints;
  if (!scriptURL.isEmpty()) {
    protocol::DictionaryValue* breakpointsByUrl =
        m_state->getObject(DebuggerAgentState::breakpointsByUrl);
    if (breakpointsByUrl) {
      potentialBreakpoints.push_back(breakpointsByUrl->getObject(scriptURL));
    }
    potentialBreakpoints.push_back(
        m_state->getObject(DebuggerAgentState::breakpointsByRegex));
  }
  protocol::DictionaryValue* breakpointsByScriptHash =
      m_state->getObject(DebuggerAgentState::breakpointsByScriptHash);
  if (breakpointsByScriptHash) {
    potentialBreakpoints.push_back(
        breakpointsByScriptHash->getObject(scriptRef->hash()));
  }
  protocol::DictionaryValue* breakpointHints =
      m_state->getObject(DebuggerAgentState::breakpointHints);
  for (auto breakpoints : potentialBreakpoints) {
    if (!breakpoints) continue;
    for (size_t i = 0; i < breakpoints->size(); ++i) {
      auto breakpointWithCondition = breakpoints->at(i);
      String16 breakpointId = breakpointWithCondition.first;

      BreakpointType type;
      String16 selector;
      int lineNumber = 0;
      int columnNumber = 0;
      parseBreakpointId(breakpointId, &type, &selector, &lineNumber,
                        &columnNumber);

      if (!matches(m_inspector, *scriptRef, type, selector)) continue;
      String16 condition;
      breakpointWithCondition.second->asString(&condition);
      String16 hint;
      bool hasHint =
          breakpointHints && breakpointHints->getString(breakpointId, &hint);
      if (hasHint) {
        adjustBreakpointLocation(*scriptRef, hint, &lineNumber, &columnNumber);
      }
      std::unique_ptr<protocol::Debugger::Location> location =
          setBreakpointImpl(breakpointId, scriptId, condition, lineNumber,
                            columnNumber);
      if (location)
        m_frontend.breakpointResolved(breakpointId, std::move(location));
    }
  }
  setScriptInstrumentationBreakpointIfNeeded(scriptRef);
}

void V8DebuggerAgentImpl::setScriptInstrumentationBreakpointIfNeeded(
    V8DebuggerScript* scriptRef) {
  protocol::DictionaryValue* breakpoints =
      m_state->getObject(DebuggerAgentState::instrumentationBreakpoints);
  if (!breakpoints) return;
  bool isBlackboxed = isFunctionBlackboxed(
      scriptRef->scriptId(), v8::debug::Location(0, 0),
      v8::debug::Location(scriptRef->endLine(), scriptRef->endColumn()));
  if (isBlackboxed) return;

  String16 sourceMapURL = scriptRef->sourceMappingURL();
  String16 breakpointId = generateInstrumentationBreakpointId(
      InstrumentationEnum::BeforeScriptExecution);
  if (!breakpoints->get(breakpointId)) {
    if (sourceMapURL.isEmpty()) return;
    breakpointId = generateInstrumentationBreakpointId(
        InstrumentationEnum::BeforeScriptWithSourceMapExecution);
    if (!breakpoints->get(breakpointId)) return;
  }
  v8::debug::BreakpointId debuggerBreakpointId;
  if (!scriptRef->setBreakpointOnRun(&debuggerBreakpointId)) return;
  std::unique_ptr<protocol::DictionaryValue> data =
      protocol::DictionaryValue::create();
  data->setString("url", scriptRef->sourceURL());
  data->setString("scriptId", scriptRef->scriptId());
  if (!sourceMapURL.isEmpty()) data->setString("sourceMapURL", sourceMapURL);

  m_breakpointsOnScriptRun[debuggerBreakpointId] = std::move(data);
  m_debuggerBreakpointIdToBreakpointId[debuggerBreakpointId] = breakpointId;
  m_breakpointIdToDebuggerBreakpointIds[breakpointId].push_back(
      debuggerBreakpointId);
}

void V8DebuggerAgentImpl::didPause(
    int contextId, v8::Local<v8::Value> exception,
    const std::vector<v8::debug::BreakpointId>& hitBreakpoints,
    v8::debug::ExceptionType exceptionType, bool isUncaught, bool isOOMBreak,
    bool isAssert) {
  v8::HandleScope handles(m_isolate);

  std::vector<BreakReason> hitReasons;

  if (isOOMBreak) {
    hitReasons.push_back(
        std::make_pair(protocol::Debugger::Paused::ReasonEnum::OOM, nullptr));
  } else if (isAssert) {
    hitReasons.push_back(std::make_pair(
        protocol::Debugger::Paused::ReasonEnum::Assert, nullptr));
  } else if (!exception.IsEmpty()) {
    InjectedScript* injectedScript = nullptr;
    m_session->findInjectedScript(contextId, injectedScript);
    if (injectedScript) {
      String16 breakReason =
          exceptionType == v8::debug::kPromiseRejection
              ? protocol::Debugger::Paused::ReasonEnum::PromiseRejection
              : protocol::Debugger::Paused::ReasonEnum::Exception;
      std::unique_ptr<protocol::Runtime::RemoteObject> obj;
      injectedScript->wrapObject(exception, kBacktraceObjectGroup,
                                 WrapMode::kNoPreview, &obj);
      std::unique_ptr<protocol::DictionaryValue> breakAuxData;
      if (obj) {
        std::vector<uint8_t> serialized;
        obj->AppendSerialized(&serialized);
        breakAuxData = protocol::DictionaryValue::cast(
            protocol::Value::parseBinary(serialized.data(), serialized.size()));
        breakAuxData->setBoolean("uncaught", isUncaught);
      }
      hitReasons.push_back(
          std::make_pair(breakReason, std::move(breakAuxData)));
    }
  }

  auto hitBreakpointIds = std::make_unique<Array<String16>>();

  for (const auto& id : hitBreakpoints) {
    auto it = m_breakpointsOnScriptRun.find(id);
    if (it != m_breakpointsOnScriptRun.end()) {
      hitReasons.push_back(std::make_pair(
          protocol::Debugger::Paused::ReasonEnum::Instrumentation,
          std::move(it->second)));
      m_breakpointsOnScriptRun.erase(it);
      continue;
    }
    auto breakpointIterator = m_debuggerBreakpointIdToBreakpointId.find(id);
    if (breakpointIterator == m_debuggerBreakpointIdToBreakpointId.end()) {
      continue;
    }
    const String16& breakpointId = breakpointIterator->second;
    hitBreakpointIds->emplace_back(breakpointId);
    BreakpointType type;
    parseBreakpointId(breakpointId, &type);
    if (type != BreakpointType::kDebugCommand) continue;
    hitReasons.push_back(std::make_pair(
        protocol::Debugger::Paused::ReasonEnum::DebugCommand, nullptr));
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
  if (!response.IsSuccess())
    protocolCallFrames = std::make_unique<Array<CallFrame>>();

  m_frontend.paused(std::move(protocolCallFrames), breakReason,
                    std::move(breakAuxData), std::move(hitBreakpointIds),
                    currentAsyncStackTrace(), currentExternalStackTrace());
}

void V8DebuggerAgentImpl::didContinue() {
  clearBreakDetails();
  m_frontend.resumed();
  m_frontend.flush();
}

void V8DebuggerAgentImpl::breakProgram(
    const String16& breakReason,
    std::unique_ptr<protocol::DictionaryValue> data) {
  if (!enabled() || m_skipAllPauses || !m_debugger->canBreakProgram()) return;
  std::vector<BreakReason> currentScheduledReason;
  currentScheduledReason.swap(m_breakReason);
  pushBreakDetails(breakReason, std::move(data));

  int contextGroupId = m_session->contextGroupId();
  int sessionId = m_session->sessionId();
  V8InspectorImpl* inspector = m_inspector;
  m_debugger->breakProgram(contextGroupId);
  // Check that session and |this| are still around.
  if (!inspector->sessionById(contextGroupId, sessionId)) return;
  if (!enabled()) return;

  popBreakDetails();
  m_breakReason.swap(currentScheduledReason);
  if (!m_breakReason.empty()) {
    m_debugger->setPauseOnNextCall(true, m_session->contextGroupId());
  }
}

void V8DebuggerAgentImpl::setBreakpointFor(v8::Local<v8::Function> function,
                                           v8::Local<v8::String> condition,
                                           BreakpointSource source) {
  String16 breakpointId = generateBreakpointId(
      source == DebugCommandBreakpointSource ? BreakpointType::kDebugCommand
                                             : BreakpointType::kMonitorCommand,
      function);
  if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) !=
      m_breakpointIdToDebuggerBreakpointIds.end()) {
    return;
  }
  setBreakpointImpl(breakpointId, function, condition);
}

void V8DebuggerAgentImpl::removeBreakpointFor(v8::Local<v8::Function> function,
                                              BreakpointSource source) {
  String16 breakpointId = generateBreakpointId(
      source == DebugCommandBreakpointSource ? BreakpointType::kDebugCommand
                                             : BreakpointType::kMonitorCommand,
      function);
  std::vector<V8DebuggerScript*> scripts;
  removeBreakpointImpl(breakpointId, scripts);
}

void V8DebuggerAgentImpl::reset() {
  if (!enabled()) return;
  m_blackboxedPositions.clear();
  resetBlackboxedStateCache();
  m_skipList.clear();
  m_scripts.clear();
  m_cachedScriptIds.clear();
  m_cachedScriptSize = 0;
}

void V8DebuggerAgentImpl::ScriptCollected(const V8DebuggerScript* script) {
  DCHECK_NE(m_scripts.find(script->scriptId()), m_scripts.end());
  m_cachedScriptIds.push_back(script->scriptId());
  // TODO(alph): Properly calculate size when sources are one-byte strings.
  m_cachedScriptSize += script->length() * sizeof(uint16_t);

  while (m_cachedScriptSize > m_maxScriptCacheSize) {
    const String16& scriptId = m_cachedScriptIds.front();
    size_t scriptSize = m_scripts[scriptId]->length() * sizeof(uint16_t);
    DCHECK_GE(m_cachedScriptSize, scriptSize);
    m_cachedScriptSize -= scriptSize;
    m_scripts.erase(scriptId);
    m_cachedScriptIds.pop_front();
  }
}

Response V8DebuggerAgentImpl::processSkipList(
    protocol::Array<protocol::Debugger::LocationRange>* skipList) {
  std::unordered_map<String16, std::vector<std::pair<int, int>>> skipListInit;
  for (std::unique_ptr<protocol::Debugger::LocationRange>& range : *skipList) {
    protocol::Debugger::ScriptPosition* start = range->getStart();
    protocol::Debugger::ScriptPosition* end = range->getEnd();
    String16 scriptId = range->getScriptId();

    auto it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
      return Response::ServerError("No script with passed id.");

    Response res = isValidPosition(start);
    if (res.IsError()) return res;

    res = isValidPosition(end);
    if (res.IsError()) return res;

    skipListInit[scriptId].emplace_back(start->getLineNumber(),
                                        start->getColumnNumber());
    skipListInit[scriptId].emplace_back(end->getLineNumber(),
                                        end->getColumnNumber());
  }

  // Verify that the skipList is sorted, and that all ranges
  // are properly defined (start comes before end).
  for (auto skipListPair : skipListInit) {
    Response res = isValidRangeOfPositions(skipListPair.second);
    if (res.IsError()) return res;
  }

  m_skipList = std::move(skipListInit);
  return Response::Success();
}
}  // namespace v8_inspector
