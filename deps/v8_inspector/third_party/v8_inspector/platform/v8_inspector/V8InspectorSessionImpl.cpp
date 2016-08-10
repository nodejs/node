// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8InspectorSessionImpl.h"

#include "platform/inspector_protocol/Parser.h"
#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8ConsoleAgentImpl.h"
#include "platform/v8_inspector/V8Debugger.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8HeapProfilerAgentImpl.h"
#include "platform/v8_inspector/V8InspectorImpl.h"
#include "platform/v8_inspector/V8ProfilerAgentImpl.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"

namespace blink {

// static
bool V8InspectorSession::canDispatchMethod(const String16& method)
{
    return method.startWith(protocol::Runtime::Metainfo::commandPrefix)
        || method.startWith(protocol::Debugger::Metainfo::commandPrefix)
        || method.startWith(protocol::Profiler::Metainfo::commandPrefix)
        || method.startWith(protocol::HeapProfiler::Metainfo::commandPrefix)
        || method.startWith(protocol::Console::Metainfo::commandPrefix);
}

std::unique_ptr<V8InspectorSessionImpl> V8InspectorSessionImpl::create(V8InspectorImpl* inspector, int contextGroupId, protocol::FrontendChannel* channel, const String16* state)
{
    return wrapUnique(new V8InspectorSessionImpl(inspector, contextGroupId, channel, state));
}

V8InspectorSessionImpl::V8InspectorSessionImpl(V8InspectorImpl* inspector, int contextGroupId, protocol::FrontendChannel* channel, const String16* savedState)
    : m_contextGroupId(contextGroupId)
    , m_inspector(inspector)
    , m_customObjectFormatterEnabled(false)
    , m_dispatcher(channel)
    , m_state(nullptr)
    , m_runtimeAgent(nullptr)
    , m_debuggerAgent(nullptr)
    , m_heapProfilerAgent(nullptr)
    , m_profilerAgent(nullptr)
    , m_consoleAgent(nullptr)
{
    if (savedState) {
        std::unique_ptr<protocol::Value> state = protocol::parseJSON(*savedState);
        if (state)
            m_state = protocol::DictionaryValue::cast(std::move(state));
        if (!m_state)
            m_state = protocol::DictionaryValue::create();
    } else {
        m_state = protocol::DictionaryValue::create();
    }

    m_runtimeAgent = wrapUnique(new V8RuntimeAgentImpl(this, channel, agentState(protocol::Runtime::Metainfo::domainName)));
    protocol::Runtime::Dispatcher::wire(&m_dispatcher, m_runtimeAgent.get());

    m_debuggerAgent = wrapUnique(new V8DebuggerAgentImpl(this, channel, agentState(protocol::Debugger::Metainfo::domainName)));
    protocol::Debugger::Dispatcher::wire(&m_dispatcher, m_debuggerAgent.get());

    m_profilerAgent = wrapUnique(new V8ProfilerAgentImpl(this, channel, agentState(protocol::Profiler::Metainfo::domainName)));
    protocol::Profiler::Dispatcher::wire(&m_dispatcher, m_profilerAgent.get());

    m_heapProfilerAgent = wrapUnique(new V8HeapProfilerAgentImpl(this, channel, agentState(protocol::HeapProfiler::Metainfo::domainName)));
    protocol::HeapProfiler::Dispatcher::wire(&m_dispatcher, m_heapProfilerAgent.get());

    m_consoleAgent = wrapUnique(new V8ConsoleAgentImpl(this, channel, agentState(protocol::Console::Metainfo::domainName)));
    protocol::Console::Dispatcher::wire(&m_dispatcher, m_consoleAgent.get());

    if (savedState) {
        m_runtimeAgent->restore();
        m_debuggerAgent->restore();
        m_heapProfilerAgent->restore();
        m_profilerAgent->restore();
        m_consoleAgent->restore();
    }
}

V8InspectorSessionImpl::~V8InspectorSessionImpl()
{
    ErrorString errorString;
    m_consoleAgent->disable(&errorString);
    m_profilerAgent->disable(&errorString);
    m_heapProfilerAgent->disable(&errorString);
    m_debuggerAgent->disable(&errorString);
    m_runtimeAgent->disable(&errorString);

    discardInjectedScripts();
    m_inspector->disconnect(this);
}

protocol::DictionaryValue* V8InspectorSessionImpl::agentState(const String16& name)
{
    protocol::DictionaryValue* state = m_state->getObject(name);
    if (!state) {
        std::unique_ptr<protocol::DictionaryValue> newState = protocol::DictionaryValue::create();
        state = newState.get();
        m_state->setObject(name, std::move(newState));
    }
    return state;
}

void V8InspectorSessionImpl::reset()
{
    m_debuggerAgent->reset();
    m_runtimeAgent->reset();
    discardInjectedScripts();
}

void V8InspectorSessionImpl::discardInjectedScripts()
{
    m_inspectedObjects.clear();
    const V8InspectorImpl::ContextByIdMap* contexts = m_inspector->contextGroup(m_contextGroupId);
    if (!contexts)
        return;

    std::vector<int> keys;
    keys.reserve(contexts->size());
    for (auto& idContext : *contexts)
        keys.push_back(idContext.first);
    for (auto& key : keys) {
        contexts = m_inspector->contextGroup(m_contextGroupId);
        if (!contexts)
            continue;
        auto contextIt = contexts->find(key);
        if (contextIt != contexts->end())
            contextIt->second->discardInjectedScript(); // This may destroy some contexts.
    }
}

InjectedScript* V8InspectorSessionImpl::findInjectedScript(ErrorString* errorString, int contextId)
{
    if (!contextId) {
        *errorString = "Cannot find context with specified id";
        return nullptr;
    }

    const V8InspectorImpl::ContextByIdMap* contexts = m_inspector->contextGroup(m_contextGroupId);
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
        context->createInjectedScript();
        if (!context->getInjectedScript()) {
            *errorString = "Cannot access specified execution context";
            return nullptr;
        }
        if (m_customObjectFormatterEnabled)
            context->getInjectedScript()->setCustomObjectFormatterEnabled(true);
    }
    return context->getInjectedScript();
}

InjectedScript* V8InspectorSessionImpl::findInjectedScript(ErrorString* errorString, RemoteObjectIdBase* objectId)
{
    return objectId ? findInjectedScript(errorString, objectId->contextId()) : nullptr;
}

void V8InspectorSessionImpl::releaseObjectGroup(const String16& objectGroup)
{
    const V8InspectorImpl::ContextByIdMap* contexts = m_inspector->contextGroup(m_contextGroupId);
    if (!contexts)
        return;

    std::vector<int> keys;
    for (auto& idContext : *contexts)
        keys.push_back(idContext.first);
    for (auto& key : keys) {
        contexts = m_inspector->contextGroup(m_contextGroupId);
        if (!contexts)
            continue;
        auto contextsIt = contexts->find(key);
        if (contextsIt == contexts->end())
            continue;
        InjectedScript* injectedScript = contextsIt->second->getInjectedScript();
        if (injectedScript)
            injectedScript->releaseObjectGroup(objectGroup); // This may destroy some contexts.
    }
}

bool V8InspectorSessionImpl::unwrapObject(ErrorString* errorString, const String16& objectId, v8::Local<v8::Value>* object, v8::Local<v8::Context>* context, String16* objectGroup)
{
    std::unique_ptr<RemoteObjectId> remoteId = RemoteObjectId::parse(errorString, objectId);
    if (!remoteId)
        return false;
    InjectedScript* injectedScript = findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return false;
    if (!injectedScript->findObject(errorString, *remoteId, object))
        return false;
    *context = injectedScript->context()->context();
    *objectGroup = injectedScript->objectGroupName(*remoteId);
    return true;
}

std::unique_ptr<protocol::Runtime::API::RemoteObject> V8InspectorSessionImpl::wrapObject(v8::Local<v8::Context> context, v8::Local<v8::Value> value, const String16& groupName)
{
    return wrapObject(context, value, groupName, false);
}

std::unique_ptr<protocol::Runtime::RemoteObject> V8InspectorSessionImpl::wrapObject(v8::Local<v8::Context> context, v8::Local<v8::Value> value, const String16& groupName, bool generatePreview)
{
    ErrorString errorString;
    InjectedScript* injectedScript = findInjectedScript(&errorString, V8Debugger::contextId(context));
    if (!injectedScript)
        return nullptr;
    return injectedScript->wrapObject(&errorString, value, groupName, false, generatePreview);
}

std::unique_ptr<protocol::Runtime::RemoteObject> V8InspectorSessionImpl::wrapTable(v8::Local<v8::Context> context, v8::Local<v8::Value> table, v8::Local<v8::Value> columns)
{
    ErrorString errorString;
    InjectedScript* injectedScript = findInjectedScript(&errorString, V8Debugger::contextId(context));
    if (!injectedScript)
        return nullptr;
    return injectedScript->wrapTable(table, columns);
}

void V8InspectorSessionImpl::setCustomObjectFormatterEnabled(bool enabled)
{
    m_customObjectFormatterEnabled = enabled;
    const V8InspectorImpl::ContextByIdMap* contexts = m_inspector->contextGroup(m_contextGroupId);
    if (!contexts)
        return;
    for (auto& idContext : *contexts) {
        InjectedScript* injectedScript = idContext.second->getInjectedScript();
        if (injectedScript)
            injectedScript->setCustomObjectFormatterEnabled(enabled);
    }
}

void V8InspectorSessionImpl::reportAllContexts(V8RuntimeAgentImpl* agent)
{
    const V8InspectorImpl::ContextByIdMap* contexts = m_inspector->contextGroup(m_contextGroupId);
    if (!contexts)
        return;
    for (auto& idContext : *contexts)
        agent->reportExecutionContextCreated(idContext.second.get());
}

void V8InspectorSessionImpl::dispatchProtocolMessage(const String16& message)
{
    m_dispatcher.dispatch(message);
}

String16 V8InspectorSessionImpl::stateJSON()
{
    return m_state->toJSONString();
}

void V8InspectorSessionImpl::addInspectedObject(std::unique_ptr<V8InspectorSession::Inspectable> inspectable)
{
    m_inspectedObjects.insert(m_inspectedObjects.begin(), std::move(inspectable));
    if (m_inspectedObjects.size() > kInspectedObjectBufferSize)
        m_inspectedObjects.resize(kInspectedObjectBufferSize);
}

V8InspectorSession::Inspectable* V8InspectorSessionImpl::inspectedObject(unsigned num)
{
    if (num >= m_inspectedObjects.size())
        return nullptr;
    return m_inspectedObjects[num].get();
}

void V8InspectorSessionImpl::schedulePauseOnNextStatement(const String16& breakReason, const String16& breakDetails)
{
    m_debuggerAgent->schedulePauseOnNextStatement(breakReason, protocol::DictionaryValue::cast(parseJSON(breakDetails)));
}

void V8InspectorSessionImpl::cancelPauseOnNextStatement()
{
    m_debuggerAgent->cancelPauseOnNextStatement();
}

void V8InspectorSessionImpl::breakProgram(const String16& breakReason, const String16& breakDetails)
{
    m_debuggerAgent->breakProgram(breakReason, protocol::DictionaryValue::cast(parseJSON(breakDetails)));
}

void V8InspectorSessionImpl::setSkipAllPauses(bool skip)
{
    ErrorString errorString;
    m_debuggerAgent->setSkipAllPauses(&errorString, skip);
}

void V8InspectorSessionImpl::resume()
{
    ErrorString errorString;
    m_debuggerAgent->resume(&errorString);
}

void V8InspectorSessionImpl::stepOver()
{
    ErrorString errorString;
    m_debuggerAgent->stepOver(&errorString);
}

std::unique_ptr<protocol::Array<protocol::Debugger::API::SearchMatch>> V8InspectorSessionImpl::searchInTextByLines(const String16& text, const String16& query, bool caseSensitive, bool isRegex)
{
    std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> matches = searchInTextByLinesImpl(this, text, query, caseSensitive, isRegex);
    std::unique_ptr<protocol::Array<protocol::Debugger::API::SearchMatch>> result = protocol::Array<protocol::Debugger::API::SearchMatch>::create();
    for (size_t i = 0; i < matches.size(); ++i)
        result->addItem(std::move(matches[i]));
    return result;
}

} // namespace blink
