// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-inspector-session-impl.h"

#include "src/inspector/injected-script.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/remote-object-id.h"
#include "src/inspector/search-util.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console-agent-impl.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-heap-profiler-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-profiler-agent-impl.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-schema-agent-impl.h"

namespace v8_inspector {

// static
bool V8InspectorSession::canDispatchMethod(const StringView& method) {
  return stringViewStartsWith(method,
                              protocol::Runtime::Metainfo::commandPrefix) ||
         stringViewStartsWith(method,
                              protocol::Debugger::Metainfo::commandPrefix) ||
         stringViewStartsWith(method,
                              protocol::Profiler::Metainfo::commandPrefix) ||
         stringViewStartsWith(
             method, protocol::HeapProfiler::Metainfo::commandPrefix) ||
         stringViewStartsWith(method,
                              protocol::Console::Metainfo::commandPrefix) ||
         stringViewStartsWith(method,
                              protocol::Schema::Metainfo::commandPrefix);
}

std::unique_ptr<V8InspectorSessionImpl> V8InspectorSessionImpl::create(
    V8InspectorImpl* inspector, int contextGroupId,
    V8Inspector::Channel* channel, const StringView& state) {
  return wrapUnique(
      new V8InspectorSessionImpl(inspector, contextGroupId, channel, state));
}

V8InspectorSessionImpl::V8InspectorSessionImpl(V8InspectorImpl* inspector,
                                               int contextGroupId,
                                               V8Inspector::Channel* channel,
                                               const StringView& savedState)
    : m_contextGroupId(contextGroupId),
      m_inspector(inspector),
      m_channel(channel),
      m_customObjectFormatterEnabled(false),
      m_dispatcher(this),
      m_state(nullptr),
      m_runtimeAgent(nullptr),
      m_debuggerAgent(nullptr),
      m_heapProfilerAgent(nullptr),
      m_profilerAgent(nullptr),
      m_consoleAgent(nullptr),
      m_schemaAgent(nullptr) {
  if (savedState.length()) {
    std::unique_ptr<protocol::Value> state =
        protocol::parseJSON(toString16(savedState));
    if (state) m_state = protocol::DictionaryValue::cast(std::move(state));
    if (!m_state) m_state = protocol::DictionaryValue::create();
  } else {
    m_state = protocol::DictionaryValue::create();
  }

  m_runtimeAgent = wrapUnique(new V8RuntimeAgentImpl(
      this, this, agentState(protocol::Runtime::Metainfo::domainName)));
  protocol::Runtime::Dispatcher::wire(&m_dispatcher, m_runtimeAgent.get());

  m_debuggerAgent = wrapUnique(new V8DebuggerAgentImpl(
      this, this, agentState(protocol::Debugger::Metainfo::domainName)));
  protocol::Debugger::Dispatcher::wire(&m_dispatcher, m_debuggerAgent.get());

  m_profilerAgent = wrapUnique(new V8ProfilerAgentImpl(
      this, this, agentState(protocol::Profiler::Metainfo::domainName)));
  protocol::Profiler::Dispatcher::wire(&m_dispatcher, m_profilerAgent.get());

  m_heapProfilerAgent = wrapUnique(new V8HeapProfilerAgentImpl(
      this, this, agentState(protocol::HeapProfiler::Metainfo::domainName)));
  protocol::HeapProfiler::Dispatcher::wire(&m_dispatcher,
                                           m_heapProfilerAgent.get());

  m_consoleAgent = wrapUnique(new V8ConsoleAgentImpl(
      this, this, agentState(protocol::Console::Metainfo::domainName)));
  protocol::Console::Dispatcher::wire(&m_dispatcher, m_consoleAgent.get());

  m_schemaAgent = wrapUnique(new V8SchemaAgentImpl(
      this, this, agentState(protocol::Schema::Metainfo::domainName)));
  protocol::Schema::Dispatcher::wire(&m_dispatcher, m_schemaAgent.get());

  if (savedState.length()) {
    m_runtimeAgent->restore();
    m_debuggerAgent->restore();
    m_heapProfilerAgent->restore();
    m_profilerAgent->restore();
    m_consoleAgent->restore();
  }
}

V8InspectorSessionImpl::~V8InspectorSessionImpl() {
  ErrorString errorString;
  m_consoleAgent->disable(&errorString);
  m_profilerAgent->disable(&errorString);
  m_heapProfilerAgent->disable(&errorString);
  m_debuggerAgent->disable(&errorString);
  m_runtimeAgent->disable(&errorString);

  discardInjectedScripts();
  m_inspector->disconnect(this);
}

protocol::DictionaryValue* V8InspectorSessionImpl::agentState(
    const String16& name) {
  protocol::DictionaryValue* state = m_state->getObject(name);
  if (!state) {
    std::unique_ptr<protocol::DictionaryValue> newState =
        protocol::DictionaryValue::create();
    state = newState.get();
    m_state->setObject(name, std::move(newState));
  }
  return state;
}

void V8InspectorSessionImpl::sendProtocolResponse(int callId,
                                                  const String16& message) {
  m_channel->sendProtocolResponse(callId, toStringView(message));
}

void V8InspectorSessionImpl::sendProtocolNotification(const String16& message) {
  m_channel->sendProtocolNotification(toStringView(message));
}

void V8InspectorSessionImpl::flushProtocolNotifications() {
  m_channel->flushProtocolNotifications();
}

void V8InspectorSessionImpl::reset() {
  m_debuggerAgent->reset();
  m_runtimeAgent->reset();
  discardInjectedScripts();
}

void V8InspectorSessionImpl::discardInjectedScripts() {
  m_inspectedObjects.clear();
  const V8InspectorImpl::ContextByIdMap* contexts =
      m_inspector->contextGroup(m_contextGroupId);
  if (!contexts) return;

  std::vector<int> keys;
  keys.reserve(contexts->size());
  for (auto& idContext : *contexts) keys.push_back(idContext.first);
  for (auto& key : keys) {
    contexts = m_inspector->contextGroup(m_contextGroupId);
    if (!contexts) continue;
    auto contextIt = contexts->find(key);
    if (contextIt != contexts->end())
      contextIt->second
          ->discardInjectedScript();  // This may destroy some contexts.
  }
}

InjectedScript* V8InspectorSessionImpl::findInjectedScript(
    ErrorString* errorString, int contextId) {
  if (!contextId) {
    *errorString = "Cannot find context with specified id";
    return nullptr;
  }

  const V8InspectorImpl::ContextByIdMap* contexts =
      m_inspector->contextGroup(m_contextGroupId);
  if (!contexts) {
    *errorString = "Cannot find context with specified id";
    return nullptr;
  }

  auto contextsIt = contexts->find(contextId);
  if (contextsIt == contexts->end()) {
    *errorString = "Cannot find context with specified id";
    return nullptr;
  }

  const std::unique_ptr<InspectedContext>& context = contextsIt->second;
  if (!context->getInjectedScript()) {
    if (!context->createInjectedScript()) {
      *errorString = "Cannot access specified execution context";
      return nullptr;
    }
    if (m_customObjectFormatterEnabled)
      context->getInjectedScript()->setCustomObjectFormatterEnabled(true);
  }
  return context->getInjectedScript();
}

InjectedScript* V8InspectorSessionImpl::findInjectedScript(
    ErrorString* errorString, RemoteObjectIdBase* objectId) {
  return objectId ? findInjectedScript(errorString, objectId->contextId())
                  : nullptr;
}

void V8InspectorSessionImpl::releaseObjectGroup(const StringView& objectGroup) {
  releaseObjectGroup(toString16(objectGroup));
}

void V8InspectorSessionImpl::releaseObjectGroup(const String16& objectGroup) {
  const V8InspectorImpl::ContextByIdMap* contexts =
      m_inspector->contextGroup(m_contextGroupId);
  if (!contexts) return;

  std::vector<int> keys;
  for (auto& idContext : *contexts) keys.push_back(idContext.first);
  for (auto& key : keys) {
    contexts = m_inspector->contextGroup(m_contextGroupId);
    if (!contexts) continue;
    auto contextsIt = contexts->find(key);
    if (contextsIt == contexts->end()) continue;
    InjectedScript* injectedScript = contextsIt->second->getInjectedScript();
    if (injectedScript)
      injectedScript->releaseObjectGroup(
          objectGroup);  // This may destroy some contexts.
  }
}

bool V8InspectorSessionImpl::unwrapObject(
    std::unique_ptr<StringBuffer>* error, const StringView& objectId,
    v8::Local<v8::Value>* object, v8::Local<v8::Context>* context,
    std::unique_ptr<StringBuffer>* objectGroup) {
  ErrorString errorString;
  String16 objectGroupString;
  bool result =
      unwrapObject(&errorString, toString16(objectId), object, context,
                   objectGroup ? &objectGroupString : nullptr);
  if (error) *error = StringBufferImpl::adopt(errorString);
  if (objectGroup) *objectGroup = StringBufferImpl::adopt(objectGroupString);
  return result;
}

bool V8InspectorSessionImpl::unwrapObject(ErrorString* errorString,
                                          const String16& objectId,
                                          v8::Local<v8::Value>* object,
                                          v8::Local<v8::Context>* context,
                                          String16* objectGroup) {
  std::unique_ptr<RemoteObjectId> remoteId =
      RemoteObjectId::parse(errorString, objectId);
  if (!remoteId) return false;
  InjectedScript* injectedScript =
      findInjectedScript(errorString, remoteId.get());
  if (!injectedScript) return false;
  if (!injectedScript->findObject(errorString, *remoteId, object)) return false;
  *context = injectedScript->context()->context();
  if (objectGroup) *objectGroup = injectedScript->objectGroupName(*remoteId);
  return true;
}

std::unique_ptr<protocol::Runtime::API::RemoteObject>
V8InspectorSessionImpl::wrapObject(v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value,
                                   const StringView& groupName) {
  return wrapObject(context, value, toString16(groupName), false);
}

std::unique_ptr<protocol::Runtime::RemoteObject>
V8InspectorSessionImpl::wrapObject(v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value,
                                   const String16& groupName,
                                   bool generatePreview) {
  ErrorString errorString;
  InjectedScript* injectedScript =
      findInjectedScript(&errorString, V8Debugger::contextId(context));
  if (!injectedScript) return nullptr;
  return injectedScript->wrapObject(&errorString, value, groupName, false,
                                    generatePreview);
}

std::unique_ptr<protocol::Runtime::RemoteObject>
V8InspectorSessionImpl::wrapTable(v8::Local<v8::Context> context,
                                  v8::Local<v8::Value> table,
                                  v8::Local<v8::Value> columns) {
  ErrorString errorString;
  InjectedScript* injectedScript =
      findInjectedScript(&errorString, V8Debugger::contextId(context));
  if (!injectedScript) return nullptr;
  return injectedScript->wrapTable(table, columns);
}

void V8InspectorSessionImpl::setCustomObjectFormatterEnabled(bool enabled) {
  m_customObjectFormatterEnabled = enabled;
  const V8InspectorImpl::ContextByIdMap* contexts =
      m_inspector->contextGroup(m_contextGroupId);
  if (!contexts) return;
  for (auto& idContext : *contexts) {
    InjectedScript* injectedScript = idContext.second->getInjectedScript();
    if (injectedScript)
      injectedScript->setCustomObjectFormatterEnabled(enabled);
  }
}

void V8InspectorSessionImpl::reportAllContexts(V8RuntimeAgentImpl* agent) {
  const V8InspectorImpl::ContextByIdMap* contexts =
      m_inspector->contextGroup(m_contextGroupId);
  if (!contexts) return;
  for (auto& idContext : *contexts)
    agent->reportExecutionContextCreated(idContext.second.get());
}

void V8InspectorSessionImpl::dispatchProtocolMessage(
    const StringView& message) {
  m_dispatcher.dispatch(protocol::parseJSON(message));
}

std::unique_ptr<StringBuffer> V8InspectorSessionImpl::stateJSON() {
  String16 json = m_state->toJSONString();
  return StringBufferImpl::adopt(json);
}

std::vector<std::unique_ptr<protocol::Schema::API::Domain>>
V8InspectorSessionImpl::supportedDomains() {
  std::vector<std::unique_ptr<protocol::Schema::Domain>> domains =
      supportedDomainsImpl();
  std::vector<std::unique_ptr<protocol::Schema::API::Domain>> result;
  for (size_t i = 0; i < domains.size(); ++i)
    result.push_back(std::move(domains[i]));
  return result;
}

std::vector<std::unique_ptr<protocol::Schema::Domain>>
V8InspectorSessionImpl::supportedDomainsImpl() {
  std::vector<std::unique_ptr<protocol::Schema::Domain>> result;
  result.push_back(protocol::Schema::Domain::create()
                       .setName(protocol::Runtime::Metainfo::domainName)
                       .setVersion(protocol::Runtime::Metainfo::version)
                       .build());
  result.push_back(protocol::Schema::Domain::create()
                       .setName(protocol::Debugger::Metainfo::domainName)
                       .setVersion(protocol::Debugger::Metainfo::version)
                       .build());
  result.push_back(protocol::Schema::Domain::create()
                       .setName(protocol::Profiler::Metainfo::domainName)
                       .setVersion(protocol::Profiler::Metainfo::version)
                       .build());
  result.push_back(protocol::Schema::Domain::create()
                       .setName(protocol::HeapProfiler::Metainfo::domainName)
                       .setVersion(protocol::HeapProfiler::Metainfo::version)
                       .build());
  result.push_back(protocol::Schema::Domain::create()
                       .setName(protocol::Schema::Metainfo::domainName)
                       .setVersion(protocol::Schema::Metainfo::version)
                       .build());
  return result;
}

void V8InspectorSessionImpl::addInspectedObject(
    std::unique_ptr<V8InspectorSession::Inspectable> inspectable) {
  m_inspectedObjects.insert(m_inspectedObjects.begin(), std::move(inspectable));
  if (m_inspectedObjects.size() > kInspectedObjectBufferSize)
    m_inspectedObjects.resize(kInspectedObjectBufferSize);
}

V8InspectorSession::Inspectable* V8InspectorSessionImpl::inspectedObject(
    unsigned num) {
  if (num >= m_inspectedObjects.size()) return nullptr;
  return m_inspectedObjects[num].get();
}

void V8InspectorSessionImpl::schedulePauseOnNextStatement(
    const StringView& breakReason, const StringView& breakDetails) {
  m_debuggerAgent->schedulePauseOnNextStatement(
      toString16(breakReason),
      protocol::DictionaryValue::cast(protocol::parseJSON(breakDetails)));
}

void V8InspectorSessionImpl::cancelPauseOnNextStatement() {
  m_debuggerAgent->cancelPauseOnNextStatement();
}

void V8InspectorSessionImpl::breakProgram(const StringView& breakReason,
                                          const StringView& breakDetails) {
  m_debuggerAgent->breakProgram(
      toString16(breakReason),
      protocol::DictionaryValue::cast(protocol::parseJSON(breakDetails)));
}

void V8InspectorSessionImpl::setSkipAllPauses(bool skip) {
  ErrorString errorString;
  m_debuggerAgent->setSkipAllPauses(&errorString, skip);
}

void V8InspectorSessionImpl::resume() {
  ErrorString errorString;
  m_debuggerAgent->resume(&errorString);
}

void V8InspectorSessionImpl::stepOver() {
  ErrorString errorString;
  m_debuggerAgent->stepOver(&errorString);
}

std::vector<std::unique_ptr<protocol::Debugger::API::SearchMatch>>
V8InspectorSessionImpl::searchInTextByLines(const StringView& text,
                                            const StringView& query,
                                            bool caseSensitive, bool isRegex) {
  // TODO(dgozman): search may operate on StringView and avoid copying |text|.
  std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> matches =
      searchInTextByLinesImpl(this, toString16(text), toString16(query),
                              caseSensitive, isRegex);
  std::vector<std::unique_ptr<protocol::Debugger::API::SearchMatch>> result;
  for (size_t i = 0; i < matches.size(); ++i)
    result.push_back(std::move(matches[i]));
  return result;
}

}  // namespace v8_inspector
