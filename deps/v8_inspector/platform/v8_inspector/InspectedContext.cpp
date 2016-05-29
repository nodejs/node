// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/InspectedContext.h"

#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/V8Console.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

void InspectedContext::weakCallback(const v8::WeakCallbackInfo<InspectedContext>& data)
{
    InspectedContext* context = data.GetParameter();
    if (!context->m_context.IsEmpty()) {
        context->m_context.Reset();
        data.SetSecondPassCallback(&InspectedContext::weakCallback);
    } else {
        context->m_debugger->discardInspectedContext(context->m_contextGroupId, context->m_contextId);
    }
}

void InspectedContext::consoleWeakCallback(const v8::WeakCallbackInfo<InspectedContext>& data)
{
    data.GetParameter()->m_console.Reset();
}

InspectedContext::InspectedContext(V8DebuggerImpl* debugger, const V8ContextInfo& info, int contextId)
    : m_debugger(debugger)
    , m_context(info.context->GetIsolate(), info.context)
    , m_contextId(contextId)
    , m_contextGroupId(info.contextGroupId)
    , m_isDefault(info.isDefault)
    , m_origin(info.origin)
    , m_humanReadableName(info.humanReadableName)
    , m_frameId(info.frameId)
    , m_reported(false)
{
    m_context.SetWeak(this, &InspectedContext::weakCallback, v8::WeakCallbackType::kParameter);

    v8::Isolate* isolate = m_debugger->isolate();
    v8::Local<v8::Object> global = info.context->Global();
    v8::Local<v8::Object> console = V8Console::createConsole(this, info.hasMemoryOnConsole);
    if (!global->Set(info.context, toV8StringInternalized(isolate, "console"), console).FromMaybe(false))
        return;
    m_console.Reset(isolate, console);
    m_console.SetWeak(this, &InspectedContext::consoleWeakCallback, v8::WeakCallbackType::kParameter);
}

InspectedContext::~InspectedContext()
{
    if (!m_context.IsEmpty() && !m_console.IsEmpty()) {
        v8::HandleScope scope(isolate());
        V8Console::clearInspectedContextIfNeeded(context(), m_console.Get(isolate()));
    }
}

v8::Local<v8::Context> InspectedContext::context() const
{
    return m_context.Get(isolate());
}

v8::Isolate* InspectedContext::isolate() const
{
    return m_debugger->isolate();
}

void InspectedContext::createInjectedScript()
{
    DCHECK(!m_injectedScript);
    v8::HandleScope handles(isolate());
    v8::Local<v8::Context> localContext = context();
    v8::Local<v8::Context> callingContext = isolate()->GetCallingContext();
    if (!callingContext.IsEmpty() && !m_debugger->client()->callingContextCanAccessContext(callingContext, localContext))
        return;
    m_injectedScript = InjectedScript::create(this);
}

void InspectedContext::discardInjectedScript()
{
    m_injectedScript.reset();
}

} // namespace blink
