/*
 * Copyright (c) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/v8_inspector/V8InspectorImpl.h"

#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/V8ConsoleAgentImpl.h"
#include "platform/v8_inspector/V8ConsoleMessage.h"
#include "platform/v8_inspector/V8Debugger.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"
#include <v8-profiler.h>

namespace v8_inspector {

std::unique_ptr<V8Inspector> V8Inspector::create(v8::Isolate* isolate, V8InspectorClient* client)
{
    return wrapUnique(new V8InspectorImpl(isolate, client));
}

V8InspectorImpl::V8InspectorImpl(v8::Isolate* isolate, V8InspectorClient* client)
    : m_isolate(isolate)
    , m_client(client)
    , m_debugger(new V8Debugger(isolate, this))
    , m_capturingStackTracesCount(0)
    , m_lastExceptionId(0)
{
}

V8InspectorImpl::~V8InspectorImpl()
{
}

V8DebuggerAgentImpl* V8InspectorImpl::enabledDebuggerAgentForGroup(int contextGroupId)
{
    V8InspectorSessionImpl* session = sessionForContextGroup(contextGroupId);
    V8DebuggerAgentImpl* agent = session ? session->debuggerAgent() : nullptr;
    return agent && agent->enabled() ? agent : nullptr;
}

V8RuntimeAgentImpl* V8InspectorImpl::enabledRuntimeAgentForGroup(int contextGroupId)
{
    V8InspectorSessionImpl* session = sessionForContextGroup(contextGroupId);
    V8RuntimeAgentImpl* agent = session ? session->runtimeAgent() : nullptr;
    return agent && agent->enabled() ? agent : nullptr;
}

v8::MaybeLocal<v8::Value> V8InspectorImpl::runCompiledScript(v8::Local<v8::Context> context, v8::Local<v8::Script> script)
{
    v8::MicrotasksScope microtasksScope(m_isolate, v8::MicrotasksScope::kRunMicrotasks);
    int groupId = V8Debugger::getGroupId(context);
    if (V8DebuggerAgentImpl* agent = enabledDebuggerAgentForGroup(groupId))
        agent->willExecuteScript(script->GetUnboundScript()->GetId());
    v8::MaybeLocal<v8::Value> result = script->Run(context);
    // Get agent from the map again, since it could have detached during script execution.
    if (V8DebuggerAgentImpl* agent = enabledDebuggerAgentForGroup(groupId))
        agent->didExecuteScript();
    return result;
}

v8::MaybeLocal<v8::Value> V8InspectorImpl::callFunction(v8::Local<v8::Function> function, v8::Local<v8::Context> context, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    v8::MicrotasksScope microtasksScope(m_isolate, v8::MicrotasksScope::kRunMicrotasks);
    int groupId = V8Debugger::getGroupId(context);
    if (V8DebuggerAgentImpl* agent = enabledDebuggerAgentForGroup(groupId))
        agent->willExecuteScript(function->ScriptId());
    v8::MaybeLocal<v8::Value> result = function->Call(context, receiver, argc, info);
    // Get agent from the map again, since it could have detached during script execution.
    if (V8DebuggerAgentImpl* agent = enabledDebuggerAgentForGroup(groupId))
        agent->didExecuteScript();
    return result;
}

v8::MaybeLocal<v8::Value> V8InspectorImpl::compileAndRunInternalScript(v8::Local<v8::Context> context, v8::Local<v8::String> source)
{
    v8::Local<v8::Script> script = compileScript(context, source, String16(), true);
    if (script.IsEmpty())
        return v8::MaybeLocal<v8::Value>();
    v8::MicrotasksScope microtasksScope(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    return script->Run(context);
}

v8::Local<v8::Script> V8InspectorImpl::compileScript(v8::Local<v8::Context> context, v8::Local<v8::String> code, const String16& fileName, bool markAsInternal)
{
    v8::ScriptOrigin origin(
        toV8String(m_isolate, fileName),
        v8::Integer::New(m_isolate, 0),
        v8::Integer::New(m_isolate, 0),
        v8::False(m_isolate), // sharable
        v8::Local<v8::Integer>(),
        v8::Boolean::New(m_isolate, markAsInternal), // internal
        toV8String(m_isolate, String16()), // sourceMap
        v8::True(m_isolate)); // opaqueresource
    v8::ScriptCompiler::Source source(code, origin);
    v8::Local<v8::Script> script;
    if (!v8::ScriptCompiler::Compile(context, &source, v8::ScriptCompiler::kNoCompileOptions).ToLocal(&script))
        return v8::Local<v8::Script>();
    return script;
}

void V8InspectorImpl::enableStackCapturingIfNeeded()
{
    if (!m_capturingStackTracesCount)
        V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(m_isolate, true);
    ++m_capturingStackTracesCount;
}

void V8InspectorImpl::disableStackCapturingIfNeeded()
{
    if (!(--m_capturingStackTracesCount))
        V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(m_isolate, false);
}

void V8InspectorImpl::muteExceptions(int contextGroupId)
{
    m_muteExceptionsMap[contextGroupId]++;
}

void V8InspectorImpl::unmuteExceptions(int contextGroupId)
{
    m_muteExceptionsMap[contextGroupId]--;
}

V8ConsoleMessageStorage* V8InspectorImpl::ensureConsoleMessageStorage(int contextGroupId)
{
    ConsoleStorageMap::iterator storageIt = m_consoleStorageMap.find(contextGroupId);
    if (storageIt == m_consoleStorageMap.end())
        storageIt = m_consoleStorageMap.insert(std::make_pair(contextGroupId, wrapUnique(new V8ConsoleMessageStorage(this, contextGroupId)))).first;
    return storageIt->second.get();
}

std::unique_ptr<V8StackTrace> V8InspectorImpl::createStackTrace(v8::Local<v8::StackTrace> stackTrace)
{
    return m_debugger->createStackTrace(stackTrace);
}

std::unique_ptr<V8InspectorSession> V8InspectorImpl::connect(int contextGroupId, protocol::FrontendChannel* channel, const String16* state)
{
    DCHECK(m_sessions.find(contextGroupId) == m_sessions.cend());
    std::unique_ptr<V8InspectorSessionImpl> session = V8InspectorSessionImpl::create(this, contextGroupId, channel, state);
    m_sessions[contextGroupId] = session.get();
    return std::move(session);
}

void V8InspectorImpl::disconnect(V8InspectorSessionImpl* session)
{
    DCHECK(m_sessions.find(session->contextGroupId()) != m_sessions.end());
    m_sessions.erase(session->contextGroupId());
}

InspectedContext* V8InspectorImpl::getContext(int groupId, int contextId) const
{
    if (!groupId || !contextId)
        return nullptr;

    ContextsByGroupMap::const_iterator contextGroupIt = m_contexts.find(groupId);
    if (contextGroupIt == m_contexts.end())
        return nullptr;

    ContextByIdMap::iterator contextIt = contextGroupIt->second->find(contextId);
    if (contextIt == contextGroupIt->second->end())
        return nullptr;

    return contextIt->second.get();
}

void V8InspectorImpl::contextCreated(const V8ContextInfo& info)
{
    int contextId = m_debugger->markContext(info);

    ContextsByGroupMap::iterator contextIt = m_contexts.find(info.contextGroupId);
    if (contextIt == m_contexts.end())
        contextIt = m_contexts.insert(std::make_pair(info.contextGroupId, wrapUnique(new ContextByIdMap()))).first;

    const auto& contextById = contextIt->second;

    DCHECK(contextById->find(contextId) == contextById->cend());
    InspectedContext* context = new InspectedContext(this, info, contextId);
    (*contextById)[contextId] = wrapUnique(context);
    SessionMap::iterator sessionIt = m_sessions.find(info.contextGroupId);
    if (sessionIt != m_sessions.end())
        sessionIt->second->runtimeAgent()->reportExecutionContextCreated(context);
}

void V8InspectorImpl::contextDestroyed(v8::Local<v8::Context> context)
{
    int contextId = V8Debugger::contextId(context);
    int contextGroupId = V8Debugger::getGroupId(context);

    ConsoleStorageMap::iterator storageIt = m_consoleStorageMap.find(contextGroupId);
    if (storageIt != m_consoleStorageMap.end())
        storageIt->second->contextDestroyed(contextId);

    InspectedContext* inspectedContext = getContext(contextGroupId, contextId);
    if (!inspectedContext)
        return;

    SessionMap::iterator iter = m_sessions.find(contextGroupId);
    if (iter != m_sessions.end())
        iter->second->runtimeAgent()->reportExecutionContextDestroyed(inspectedContext);
    discardInspectedContext(contextGroupId, contextId);
}

void V8InspectorImpl::resetContextGroup(int contextGroupId)
{
    m_consoleStorageMap.erase(contextGroupId);
    m_muteExceptionsMap.erase(contextGroupId);
    SessionMap::iterator session = m_sessions.find(contextGroupId);
    if (session != m_sessions.end())
        session->second->reset();
    m_contexts.erase(contextGroupId);
}

void V8InspectorImpl::willExecuteScript(v8::Local<v8::Context> context, int scriptId)
{
    if (V8DebuggerAgentImpl* agent = enabledDebuggerAgentForGroup(V8Debugger::getGroupId(context)))
        agent->willExecuteScript(scriptId);
}

void V8InspectorImpl::didExecuteScript(v8::Local<v8::Context> context)
{
    if (V8DebuggerAgentImpl* agent = enabledDebuggerAgentForGroup(V8Debugger::getGroupId(context)))
        agent->didExecuteScript();
}

void V8InspectorImpl::idleStarted()
{
    m_isolate->GetCpuProfiler()->SetIdle(true);
}

void V8InspectorImpl::idleFinished()
{
    m_isolate->GetCpuProfiler()->SetIdle(false);
}

unsigned V8InspectorImpl::exceptionThrown(v8::Local<v8::Context> context, const String16& message, v8::Local<v8::Value> exception, const String16& detailedMessage, const String16& url, unsigned lineNumber, unsigned columnNumber, std::unique_ptr<V8StackTrace> stackTrace, int scriptId)
{
    int contextGroupId = V8Debugger::getGroupId(context);
    if (!contextGroupId || m_muteExceptionsMap[contextGroupId])
        return 0;
    std::unique_ptr<V8StackTraceImpl> stackTraceImpl = wrapUnique(static_cast<V8StackTraceImpl*>(stackTrace.release()));
    unsigned exceptionId = nextExceptionId();
    std::unique_ptr<V8ConsoleMessage> consoleMessage = V8ConsoleMessage::createForException(m_client->currentTimeMS(), detailedMessage, url, lineNumber, columnNumber, std::move(stackTraceImpl), scriptId, m_isolate, message, V8Debugger::contextId(context), exception, exceptionId);
    ensureConsoleMessageStorage(contextGroupId)->addMessage(std::move(consoleMessage));
    return exceptionId;
}

void V8InspectorImpl::exceptionRevoked(v8::Local<v8::Context> context, unsigned exceptionId, const String16& message)
{
    int contextGroupId = V8Debugger::getGroupId(context);
    if (!contextGroupId)
        return;

    std::unique_ptr<V8ConsoleMessage> consoleMessage = V8ConsoleMessage::createForRevokedException(m_client->currentTimeMS(), message, exceptionId);
    ensureConsoleMessageStorage(contextGroupId)->addMessage(std::move(consoleMessage));
}

std::unique_ptr<V8StackTrace> V8InspectorImpl::captureStackTrace(bool fullStack)
{
    return m_debugger->captureStackTrace(fullStack);
}

void V8InspectorImpl::asyncTaskScheduled(const String16& taskName, void* task, bool recurring)
{
    m_debugger->asyncTaskScheduled(taskName, task, recurring);
}

void V8InspectorImpl::asyncTaskCanceled(void* task)
{
    m_debugger->asyncTaskCanceled(task);
}

void V8InspectorImpl::asyncTaskStarted(void* task)
{
    m_debugger->asyncTaskStarted(task);
}

void V8InspectorImpl::asyncTaskFinished(void* task)
{
    m_debugger->asyncTaskFinished(task);
}

void V8InspectorImpl::allAsyncTasksCanceled()
{
    m_debugger->allAsyncTasksCanceled();
}

v8::Local<v8::Context> V8InspectorImpl::regexContext()
{
    if (m_regexContext.IsEmpty())
        m_regexContext.Reset(m_isolate, v8::Context::New(m_isolate));
    return m_regexContext.Get(m_isolate);
}

void V8InspectorImpl::discardInspectedContext(int contextGroupId, int contextId)
{
    if (!getContext(contextGroupId, contextId))
        return;
    m_contexts[contextGroupId]->erase(contextId);
    if (m_contexts[contextGroupId]->empty())
        m_contexts.erase(contextGroupId);
}

const V8InspectorImpl::ContextByIdMap* V8InspectorImpl::contextGroup(int contextGroupId)
{
    ContextsByGroupMap::iterator iter = m_contexts.find(contextGroupId);
    return iter == m_contexts.end() ? nullptr : iter->second.get();
}

V8InspectorSessionImpl* V8InspectorImpl::sessionForContextGroup(int contextGroupId)
{
    if (!contextGroupId)
        return nullptr;
    SessionMap::iterator iter = m_sessions.find(contextGroupId);
    return iter == m_sessions.end() ? nullptr : iter->second;
}

} // namespace v8_inspector
