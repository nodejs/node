/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#include "platform/v8_inspector/JavaScriptCallFrame.h"

#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/V8StringUtil.h"

#include <v8-debug.h>

namespace blink {

JavaScriptCallFrame::JavaScriptCallFrame(v8::Local<v8::Context> debuggerContext, v8::Local<v8::Object> callFrame)
    : m_isolate(debuggerContext->GetIsolate())
    , m_debuggerContext(m_isolate, debuggerContext)
    , m_callFrame(m_isolate, callFrame)
{
}

JavaScriptCallFrame::~JavaScriptCallFrame()
{
}

int JavaScriptCallFrame::callV8FunctionReturnInt(const char* name) const
{
    v8::HandleScope handleScope(m_isolate);
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, name)));
    v8::Local<v8::Value> result;
    if (!func->Call(context, callFrame, 0, nullptr).ToLocal(&result) || !result->IsInt32())
        return 0;
    return result.As<v8::Int32>()->Value();
}

int JavaScriptCallFrame::sourceID() const
{
    return callV8FunctionReturnInt("sourceID");
}

int JavaScriptCallFrame::line() const
{
    return callV8FunctionReturnInt("line");
}

int JavaScriptCallFrame::column() const
{
    return callV8FunctionReturnInt("column");
}

int JavaScriptCallFrame::contextId() const
{
    return callV8FunctionReturnInt("contextId");
}

bool JavaScriptCallFrame::isAtReturn() const
{
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> result = v8::Local<v8::Object>::New(m_isolate, m_callFrame)->Get(toV8StringInternalized(m_isolate, "isAtReturn"));
    if (result.IsEmpty() || !result->IsBoolean())
        return false;
    return result->BooleanValue();
}

v8::Local<v8::Object> JavaScriptCallFrame::details() const
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "details")));
    return v8::Local<v8::Object>::Cast(func->Call(m_isolate->GetCurrentContext(), callFrame, 0, nullptr).ToLocalChecked());
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::evaluate(v8::Local<v8::Value> expression)
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kRunMicrotasks);
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> evalFunction = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "evaluate")));
    return evalFunction->Call(m_isolate->GetCurrentContext(), callFrame, 1, &expression);
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::restart()
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> restartFunction = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "restart")));
    v8::Debug::SetLiveEditEnabled(m_isolate, true);
    v8::MaybeLocal<v8::Value> result = restartFunction->Call(m_isolate->GetCurrentContext(), callFrame, 0, nullptr);
    v8::Debug::SetLiveEditEnabled(m_isolate, false);
    return result;
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::setVariableValue(int scopeNumber, v8::Local<v8::Value> variableName, v8::Local<v8::Value> newValue)
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> setVariableValueFunction = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "setVariableValue")));
    v8::Local<v8::Value> argv[] = {
        v8::Local<v8::Value>(v8::Integer::New(m_isolate, scopeNumber)),
        variableName,
        newValue
    };
    return setVariableValueFunction->Call(m_isolate->GetCurrentContext(), callFrame, PROTOCOL_ARRAY_LENGTH(argv), argv);
}

} // namespace blink
