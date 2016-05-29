// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8InspectorSessionImpl.h"

#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8HeapProfilerAgentImpl.h"
#include "platform/v8_inspector/V8ProfilerAgentImpl.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

const char V8InspectorSession::backtraceObjectGroup[] = "backtrace";

std::unique_ptr<V8InspectorSessionImpl> V8InspectorSessionImpl::create(V8DebuggerImpl* debugger, int contextGroupId)
{
    return wrapUnique(new V8InspectorSessionImpl(debugger, contextGroupId));
}

V8InspectorSessionImpl::V8InspectorSessionImpl(V8DebuggerImpl* debugger, int contextGroupId)
    : m_contextGroupId(contextGroupId)
    , m_debugger(debugger)
    , m_client(nullptr)
    , m_customObjectFormatterEnabled(false)
    , m_instrumentationCounter(0)
    , m_runtimeAgent(wrapUnique(new V8RuntimeAgentImpl(this)))
    , m_debuggerAgent(wrapUnique(new V8DebuggerAgentImpl(this)))
    , m_heapProfilerAgent(wrapUnique(new V8HeapProfilerAgentImpl(this)))
    , m_profilerAgent(wrapUnique(new V8ProfilerAgentImpl(this)))
{
}

V8InspectorSessionImpl::~V8InspectorSessionImpl()
{
    discardInjectedScripts();
    m_debugger->disconnect(this);
}

V8DebuggerAgent* V8InspectorSessionImpl::debuggerAgent()
{
    return m_debuggerAgent.get();
}

V8HeapProfilerAgent* V8InspectorSessionImpl::heapProfilerAgent()
{
    return m_heapProfilerAgent.get();
}

V8ProfilerAgent* V8InspectorSessionImpl::profilerAgent()
{
    return m_profilerAgent.get();
}

V8RuntimeAgent* V8InspectorSessionImpl::runtimeAgent()
{
    return m_runtimeAgent.get();
}

void V8InspectorSessionImpl::setClient(V8InspectorSessionClient* client)
{
    m_client = client;
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
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;

    protocol::Vector<int> keys;
    for (auto& idContext : *contexts)
        keys.append(idContext.first);
    for (auto& key : keys) {
        contexts = m_debugger->contextGroup(m_contextGroupId);
        if (contexts && contexts->contains(key))
            contexts->get(key)->discardInjectedScript(); // This may destroy some contexts.
    }
}

InjectedScript* V8InspectorSessionImpl::findInjectedScript(ErrorString* errorString, int contextId)
{
    if (!contextId) {
        *errorString = "Cannot find context with specified id";
        return nullptr;
    }

    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts || !contexts->contains(contextId)) {
        *errorString = "Cannot find context with specified id";
        return nullptr;
    }

    InspectedContext* context = contexts->get(contextId);
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
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;

    protocol::Vector<int> keys;
    for (auto& idContext : *contexts)
        keys.append(idContext.first);
    for (auto& key : keys) {
        contexts = m_debugger->contextGroup(m_contextGroupId);
        if (contexts && contexts->contains(key)) {
            InjectedScript* injectedScript = contexts->get(key)->getInjectedScript();
            if (injectedScript)
                injectedScript->releaseObjectGroup(objectGroup); // This may destroy some contexts.
        }
    }
}

v8::Local<v8::Value> V8InspectorSessionImpl::findObject(ErrorString* errorString, const String16& objectId, v8::Local<v8::Context>* context, String16* groupName)
{
    std::unique_ptr<RemoteObjectId> remoteId = RemoteObjectId::parse(errorString, objectId);
    if (!remoteId)
        return v8::Local<v8::Value>();
    InjectedScript* injectedScript = findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return v8::Local<v8::Value>();
    v8::Local<v8::Value> objectValue;
    injectedScript->findObject(errorString, *remoteId, &objectValue);
    if (objectValue.IsEmpty())
        return v8::Local<v8::Value>();
    if (context)
        *context = injectedScript->context()->context();
    if (groupName)
        *groupName = injectedScript->objectGroupName(*remoteId);
    return objectValue;
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
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
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
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;
    for (auto& idContext : *contexts)
        agent->reportExecutionContextCreated(idContext.second);
}

void V8InspectorSessionImpl::changeInstrumentationCounter(int delta)
{
    DCHECK(m_instrumentationCounter + delta >= 0);
    if (!m_instrumentationCounter && m_client)
        m_client->startInstrumenting();
    m_instrumentationCounter += delta;
    if (!m_instrumentationCounter && m_client)
        m_client->stopInstrumenting();
}

void V8InspectorSessionImpl::addInspectedObject(std::unique_ptr<V8InspectorSession::Inspectable> inspectable)
{
    m_inspectedObjects.prepend(std::move(inspectable));
    while (m_inspectedObjects.size() > kInspectedObjectBufferSize)
        m_inspectedObjects.removeLast();
}

V8InspectorSession::Inspectable* V8InspectorSessionImpl::inspectedObject(unsigned num)
{
    if (num >= m_inspectedObjects.size())
        return nullptr;
    return m_inspectedObjects[num];
}

void V8InspectorSessionImpl::schedulePauseOnNextStatement(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data)
{
    m_debuggerAgent->schedulePauseOnNextStatement(breakReason, std::move(data));
}

void V8InspectorSessionImpl::cancelPauseOnNextStatement()
{
    m_debuggerAgent->cancelPauseOnNextStatement();
}

void V8InspectorSessionImpl::breakProgram(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data)
{
    m_debuggerAgent->breakProgram(breakReason, std::move(data));
}

void V8InspectorSessionImpl::breakProgramOnException(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data)
{
    m_debuggerAgent->breakProgramOnException(breakReason, std::move(data));
}

void V8InspectorSessionImpl::setSkipAllPauses(bool skip)
{
    ErrorString errorString;
    m_debuggerAgent->setSkipAllPauses(&errorString, skip);
}

void V8InspectorSessionImpl::asyncTaskScheduled(const String16& taskName, void* task, bool recurring)
{
    m_debuggerAgent->asyncTaskScheduled(taskName, task, recurring);
}

void V8InspectorSessionImpl::asyncTaskCanceled(void* task)
{
    m_debuggerAgent->asyncTaskCanceled(task);
}

void V8InspectorSessionImpl::asyncTaskStarted(void* task)
{
    m_debuggerAgent->asyncTaskStarted(task);
}

void V8InspectorSessionImpl::asyncTaskFinished(void* task)
{
    m_debuggerAgent->asyncTaskFinished(task);
}

void V8InspectorSessionImpl::allAsyncTasksCanceled()
{
    m_debuggerAgent->allAsyncTasksCanceled();
}

} // namespace blink
