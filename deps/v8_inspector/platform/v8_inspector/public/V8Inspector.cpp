// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "platform/v8_inspector/public/V8Inspector.h"

#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "platform/v8_inspector/public/V8HeapProfilerAgent.h"
#include "platform/v8_inspector/public/V8ProfilerAgent.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"

namespace blink {

V8Inspector::V8Inspector(v8::Isolate* isolate, v8::Local<v8::Context> context)
{
    m_debugger = V8Debugger::create(isolate, this);
    m_session = m_debugger->connect(1);
    m_session->setClient(this);
    m_state = protocol::DictionaryValue::create();
    m_state->setValue("runtime", protocol::DictionaryValue::create());
    m_state->setValue("debugger", protocol::DictionaryValue::create());
    m_state->setValue("profiler", protocol::DictionaryValue::create());
    m_state->setValue("heapProfiler", protocol::DictionaryValue::create());

    m_session->runtimeAgent()->setInspectorState(m_state->getObject("runtime"));
    m_session->debuggerAgent()->setInspectorState(m_state->getObject("debugger"));
    m_session->profilerAgent()->setInspectorState(m_state->getObject("profiler"));
    m_session->heapProfilerAgent()->setInspectorState(m_state->getObject("heapProfiler"));

    m_debugger->contextCreated(V8ContextInfo(context, 1, true, "",
        "NodeJS Main Context", "", false));
}

V8Inspector::~V8Inspector()
{
    disconnectFrontend();
}

void V8Inspector::eventListeners(v8::Local<v8::Value> value, V8EventListenerInfoList& result)
{
}

bool V8Inspector::callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target)
{
    return true;
}

String16 V8Inspector::valueSubtype(v8::Local<v8::Value> value)
{
    return String16();
}

bool V8Inspector::formatAccessorsAsProperties(v8::Local<v8::Value> value)
{
    return false;
}

void V8Inspector::connectFrontend(protocol::FrontendChannel* channel)
{
    DCHECK(!m_frontend);
    m_frontend = wrapUnique(new protocol::Frontend(channel));
    m_dispatcher = protocol::Dispatcher::create(channel);

    m_dispatcher->registerAgent((protocol::Backend::Debugger*)m_session->debuggerAgent());
    m_dispatcher->registerAgent(m_session->heapProfilerAgent());
    m_dispatcher->registerAgent(m_session->profilerAgent());
    m_dispatcher->registerAgent(m_session->runtimeAgent());

    m_session->debuggerAgent()->setFrontend(
        protocol::Frontend::Debugger::from(m_frontend.get()));
    m_session->heapProfilerAgent()->setFrontend(
        protocol::Frontend::HeapProfiler::from(m_frontend.get()));
    m_session->profilerAgent()->setFrontend(
        protocol::Frontend::Profiler::from(m_frontend.get()));
    m_session->runtimeAgent()->setFrontend(
        protocol::Frontend::Runtime::from(m_frontend.get()));
}

void V8Inspector::disconnectFrontend()
{
    if (!m_frontend)
        return;
    m_dispatcher->clearFrontend();
    m_dispatcher.reset();

    m_session->debuggerAgent()->clearFrontend();
    m_session->heapProfilerAgent()->clearFrontend();
    m_session->profilerAgent()->clearFrontend();
    m_session->runtimeAgent()->clearFrontend();

    m_frontend.reset();
}

void V8Inspector::dispatchMessageFromFrontend(const String16& message)
{
    if (m_dispatcher)
        m_dispatcher->dispatch(1, message);
}

int V8Inspector::ensureDefaultContextInGroup(int contextGroupId)
{
    return 1;
}

void V8Inspector::muteConsole()
{

}

void V8Inspector::unmuteConsole()
{

}

bool V8Inspector::isExecutionAllowed()
{
    return true;
}

} // namespace blink
