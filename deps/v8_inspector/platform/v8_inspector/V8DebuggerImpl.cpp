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

#include "platform/v8_inspector/V8DebuggerImpl.h"

#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/Atomics.h"
#include "platform/v8_inspector/DebuggerScript.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/ScriptBreakpoint.h"
#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include <v8-profiler.h>

namespace blink {

namespace {
const char stepIntoV8MethodName[] = "stepIntoStatement";
const char stepOutV8MethodName[] = "stepOutOfFunction";
volatile int s_lastContextId = 0;

inline v8::Local<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate)
{
    return value ? v8::True(isolate) : v8::False(isolate);
}

}

static bool inLiveEditScope = false;

v8::MaybeLocal<v8::Value> V8DebuggerImpl::callDebuggerMethod(const char* functionName, int argc, v8::Local<v8::Value> argv[])
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8InternalizedString(functionName)));
    DCHECK(m_isolate->InContext());
    return function->Call(m_isolate->GetCurrentContext(), debuggerScript, argc, argv);
}

std::unique_ptr<V8Debugger> V8Debugger::create(v8::Isolate* isolate, V8DebuggerClient* client)
{
    return wrapUnique(new V8DebuggerImpl(isolate, client));
}

V8DebuggerImpl::V8DebuggerImpl(v8::Isolate* isolate, V8DebuggerClient* client)
    : m_isolate(isolate)
    , m_client(client)
    , m_enabledAgentsCount(0)
    , m_breakpointsActivated(true)
    , m_runningNestedMessageLoop(false)
{
}

V8DebuggerImpl::~V8DebuggerImpl()
{
}

void V8DebuggerImpl::enable()
{
    DCHECK(!enabled());
    v8::HandleScope scope(m_isolate);
    v8::Debug::SetDebugEventListener(m_isolate, &V8DebuggerImpl::v8DebugEventCallback, v8::External::New(m_isolate, this));
    m_debuggerContext.Reset(m_isolate, v8::Debug::GetDebugContext(m_isolate));
    compileDebuggerScript();
}

void V8DebuggerImpl::disable()
{
    DCHECK(enabled());
    clearBreakpoints();
    m_debuggerScript.Reset();
    m_debuggerContext.Reset();
    v8::Debug::SetDebugEventListener(m_isolate, nullptr);
}

bool V8DebuggerImpl::enabled() const
{
    return !m_debuggerScript.IsEmpty();
}

int V8Debugger::contextId(v8::Local<v8::Context> context)
{
    v8::Local<v8::Value> data = context->GetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex));
    if (data.IsEmpty() || !data->IsString())
        return 0;
    String16 dataString = toProtocolString(data.As<v8::String>());
    if (dataString.isEmpty())
        return 0;
    size_t commaPos = dataString.find(",");
    if (commaPos == kNotFound)
        return 0;
    size_t commaPos2 = dataString.find(",", commaPos + 1);
    if (commaPos2 == kNotFound)
        return 0;
    return dataString.substring(commaPos + 1, commaPos2 - commaPos - 1).toInt();
}

static int getGroupId(v8::Local<v8::Context> context)
{
    v8::Local<v8::Value> data = context->GetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex));
    if (data.IsEmpty() || !data->IsString())
        return 0;
    String16 dataString = toProtocolString(data.As<v8::String>());
    if (dataString.isEmpty())
        return 0;
    size_t commaPos = dataString.find(",");
    if (commaPos == kNotFound)
        return 0;
    return dataString.substring(0, commaPos).toInt();
}

void V8DebuggerImpl::debuggerAgentEnabled()
{
    if (!m_enabledAgentsCount++)
        enable();
}

void V8DebuggerImpl::debuggerAgentDisabled()
{
    if (!--m_enabledAgentsCount)
        disable();
}

V8DebuggerAgentImpl* V8DebuggerImpl::findEnabledDebuggerAgent(int contextGroupId)
{
    if (!contextGroupId)
        return nullptr;
    V8InspectorSessionImpl* session = m_sessions.get(contextGroupId);
    if (session && session->debuggerAgentImpl()->enabled())
        return session->debuggerAgentImpl();
    return nullptr;
}

V8DebuggerAgentImpl* V8DebuggerImpl::findEnabledDebuggerAgent(v8::Local<v8::Context> context)
{
    return findEnabledDebuggerAgent(getGroupId(context));
}

void V8DebuggerImpl::getCompiledScripts(int contextGroupId, protocol::Vector<V8DebuggerParsedScript>& result)
{
    v8::HandleScope scope(m_isolate);
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
    DCHECK(!debuggerScript->IsUndefined());
    v8::Local<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8InternalizedString("getScripts")));
    v8::Local<v8::Value> argv[] = { v8::Integer::New(m_isolate, contextGroupId) };
    v8::Local<v8::Value> value;
    if (!getScriptsFunction->Call(debuggerContext(), debuggerScript, PROTOCOL_ARRAY_LENGTH(argv), argv).ToLocal(&value))
        return;
    DCHECK(value->IsArray());
    v8::Local<v8::Array> scriptsArray = v8::Local<v8::Array>::Cast(value);
    result.resize(scriptsArray->Length());
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        result[i] = createParsedScript(v8::Local<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(m_isolate, i))), true);
}

String16 V8DebuggerImpl::setBreakpoint(const String16& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber, bool interstatementLocation)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("sourceID"), toV8String(m_isolate, sourceID));
    info->Set(v8InternalizedString("lineNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.lineNumber));
    info->Set(v8InternalizedString("columnNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.columnNumber));
    info->Set(v8InternalizedString("interstatementLocation"), v8Boolean(interstatementLocation, m_isolate));
    info->Set(v8InternalizedString("condition"), toV8String(m_isolate, scriptBreakpoint.condition));

    v8::Local<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("setBreakpoint")));
    v8::Local<v8::Value> breakpointId = v8::Debug::Call(debuggerContext(), setBreakpointFunction, info).ToLocalChecked();
    if (!breakpointId->IsString())
        return "";
    *actualLineNumber = info->Get(v8InternalizedString("lineNumber"))->Int32Value();
    *actualColumnNumber = info->Get(v8InternalizedString("columnNumber"))->Int32Value();
    return toProtocolString(breakpointId.As<v8::String>());
}

void V8DebuggerImpl::removeBreakpoint(const String16& breakpointId)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("breakpointId"), toV8String(m_isolate, breakpointId));

    v8::Local<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("removeBreakpoint")));
    v8::Debug::Call(debuggerContext(), removeBreakpointFunction, info).ToLocalChecked();
}

void V8DebuggerImpl::clearBreakpoints()
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("clearBreakpoints")));
    v8::Debug::Call(debuggerContext(), clearBreakpoints).ToLocalChecked();
}

void V8DebuggerImpl::setBreakpointsActivated(bool activated)
{
    if (!enabled()) {
        NOTREACHED();
        return;
    }
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("enabled"), v8::Boolean::New(m_isolate, activated));
    v8::Local<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("setBreakpointsActivated")));
    v8::Debug::Call(debuggerContext(), setBreakpointsActivated, info).ToLocalChecked();

    m_breakpointsActivated = activated;
}

V8DebuggerImpl::PauseOnExceptionsState V8DebuggerImpl::getPauseOnExceptionsState()
{
    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Undefined(m_isolate) };
    v8::Local<v8::Value> result = callDebuggerMethod("pauseOnExceptionsState", 0, argv).ToLocalChecked();
    return static_cast<V8DebuggerImpl::PauseOnExceptionsState>(result->Int32Value());
}

void V8DebuggerImpl::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Int32::New(m_isolate, pauseOnExceptionsState) };
    callDebuggerMethod("setPauseOnExceptionsState", 1, argv);
}

void V8DebuggerImpl::setPauseOnNextStatement(bool pause)
{
    if (m_runningNestedMessageLoop)
        return;
    if (pause)
        v8::Debug::DebugBreak(m_isolate);
    else
        v8::Debug::CancelDebugBreak(m_isolate);
}

bool V8DebuggerImpl::canBreakProgram()
{
    if (!m_breakpointsActivated)
        return false;
    return m_isolate->InContext();
}

void V8DebuggerImpl::breakProgram()
{
    if (isPaused()) {
        DCHECK(!m_runningNestedMessageLoop);
        v8::Local<v8::Value> exception;
        v8::Local<v8::Array> hitBreakpoints;
        handleProgramBreak(m_pausedContext, m_executionState, exception, hitBreakpoints);
        return;
    }

    if (!canBreakProgram())
        return;

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Function> breakFunction;
    if (!v8::Function::New(m_isolate->GetCurrentContext(), &V8DebuggerImpl::breakProgramCallback, v8::External::New(m_isolate, this)).ToLocal(&breakFunction))
        return;
    v8::Debug::Call(debuggerContext(), breakFunction).ToLocalChecked();
}

void V8DebuggerImpl::continueProgram()
{
    if (isPaused())
        m_client->quitMessageLoopOnPause();
    m_pausedContext.Clear();
    m_executionState.Clear();
}

void V8DebuggerImpl::stepIntoStatement()
{
    DCHECK(isPaused());
    DCHECK(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    continueProgram();
}

void V8DebuggerImpl::stepOverStatement()
{
    DCHECK(isPaused());
    DCHECK(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod("stepOverStatement", 1, argv);
    continueProgram();
}

void V8DebuggerImpl::stepOutOfFunction()
{
    DCHECK(isPaused());
    DCHECK(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepOutV8MethodName, 1, argv);
    continueProgram();
}

void V8DebuggerImpl::clearStepping()
{
    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Undefined(m_isolate) };
    callDebuggerMethod("clearStepping", 0, argv);
}

bool V8DebuggerImpl::setScriptSource(const String16& sourceID, const String16& newContent, bool preview, ErrorString* error, Maybe<protocol::Debugger::SetScriptSourceError>* errorData, JavaScriptCallFrames* newCallFrames, Maybe<bool>* stackChanged)
{
    class EnableLiveEditScope {
    public:
        explicit EnableLiveEditScope(v8::Isolate* isolate) : m_isolate(isolate)
        {
            v8::Debug::SetLiveEditEnabled(m_isolate, true);
            inLiveEditScope = true;
        }
        ~EnableLiveEditScope()
        {
            v8::Debug::SetLiveEditEnabled(m_isolate, false);
            inLiveEditScope = false;
        }
    private:
        v8::Isolate* m_isolate;
    };

    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);

    std::unique_ptr<v8::Context::Scope> contextScope;
    if (!isPaused())
        contextScope = wrapUnique(new v8::Context::Scope(debuggerContext()));

    v8::Local<v8::Value> argv[] = { toV8String(m_isolate, sourceID), toV8String(m_isolate, newContent), v8Boolean(preview, m_isolate) };

    v8::Local<v8::Value> v8result;
    {
        EnableLiveEditScope enableLiveEditScope(m_isolate);
        v8::TryCatch tryCatch(m_isolate);
        tryCatch.SetVerbose(false);
        v8::MaybeLocal<v8::Value> maybeResult = callDebuggerMethod("liveEditScriptSource", 3, argv);
        if (tryCatch.HasCaught()) {
            v8::Local<v8::Message> message = tryCatch.Message();
            if (!message.IsEmpty())
                *error = toProtocolStringWithTypeCheck(message->Get());
            else
                *error = "Unknown error.";
            return false;
        }
        v8result = maybeResult.ToLocalChecked();
    }
    DCHECK(!v8result.IsEmpty());
    v8::Local<v8::Object> resultTuple = v8result->ToObject(m_isolate);
    int code = static_cast<int>(resultTuple->Get(0)->ToInteger(m_isolate)->Value());
    switch (code) {
    case 0:
        {
            *stackChanged = resultTuple->Get(1)->BooleanValue();
            // Call stack may have changed after if the edited function was on the stack.
            if (!preview && isPaused())
                newCallFrames->swap(currentCallFrames());
            return true;
        }
    // Compile error.
    case 1:
        {
            *errorData = protocol::Debugger::SetScriptSourceError::create()
                .setMessage(toProtocolStringWithTypeCheck(resultTuple->Get(2)))
                .setLineNumber(resultTuple->Get(3)->ToInteger(m_isolate)->Value())
                .setColumnNumber(resultTuple->Get(4)->ToInteger(m_isolate)->Value()).build();
            return false;
        }
    }
    *error = "Unknown error.";
    return false;
}

JavaScriptCallFrames V8DebuggerImpl::currentCallFrames(int limit)
{
    if (!m_isolate->InContext())
        return JavaScriptCallFrames();
    v8::Local<v8::Value> currentCallFramesV8;
    if (m_executionState.IsEmpty()) {
        v8::Local<v8::Function> currentCallFramesFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("currentCallFrames")));
        currentCallFramesV8 = v8::Debug::Call(debuggerContext(), currentCallFramesFunction, v8::Integer::New(m_isolate, limit)).ToLocalChecked();
    } else {
        v8::Local<v8::Value> argv[] = { m_executionState, v8::Integer::New(m_isolate, limit) };
        currentCallFramesV8 = callDebuggerMethod("currentCallFrames", PROTOCOL_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    }
    DCHECK(!currentCallFramesV8.IsEmpty());
    if (!currentCallFramesV8->IsArray())
        return JavaScriptCallFrames();
    v8::Local<v8::Array> callFramesArray = currentCallFramesV8.As<v8::Array>();
    JavaScriptCallFrames callFrames;
    for (size_t i = 0; i < callFramesArray->Length(); ++i) {
        v8::Local<v8::Value> callFrameValue;
        if (!callFramesArray->Get(debuggerContext(), i).ToLocal(&callFrameValue))
            return JavaScriptCallFrames();
        if (!callFrameValue->IsObject())
            return JavaScriptCallFrames();
        v8::Local<v8::Object> callFrameObject = callFrameValue.As<v8::Object>();
        callFrames.append(JavaScriptCallFrame::create(debuggerContext(), v8::Local<v8::Object>::Cast(callFrameObject)));
    }
    return callFrames;
}

static V8DebuggerImpl* toV8DebuggerImpl(v8::Local<v8::Value> data)
{
    void* p = v8::Local<v8::External>::Cast(data)->Value();
    return static_cast<V8DebuggerImpl*>(p);
}

void V8DebuggerImpl::breakProgramCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DCHECK(2 == info.Length());
    V8DebuggerImpl* thisPtr = toV8DebuggerImpl(info.Data());
    v8::Local<v8::Context> pausedContext = thisPtr->m_isolate->GetCurrentContext();
    v8::Local<v8::Value> exception;
    v8::Local<v8::Array> hitBreakpoints;
    thisPtr->handleProgramBreak(pausedContext, v8::Local<v8::Object>::Cast(info[0]), exception, hitBreakpoints);
}

void V8DebuggerImpl::handleProgramBreak(v8::Local<v8::Context> pausedContext, v8::Local<v8::Object> executionState, v8::Local<v8::Value> exception, v8::Local<v8::Array> hitBreakpointNumbers, bool isPromiseRejection)
{
    // Don't allow nested breaks.
    if (m_runningNestedMessageLoop)
        return;

    V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(pausedContext);
    if (!agent)
        return;

    protocol::Vector<String16> breakpointIds;
    if (!hitBreakpointNumbers.IsEmpty()) {
        breakpointIds.resize(hitBreakpointNumbers->Length());
        for (size_t i = 0; i < hitBreakpointNumbers->Length(); i++) {
            v8::Local<v8::Value> hitBreakpointNumber = hitBreakpointNumbers->Get(i);
            DCHECK(!hitBreakpointNumber.IsEmpty() && hitBreakpointNumber->IsInt32());
            breakpointIds[i] = String16::number(hitBreakpointNumber->Int32Value());
        }
    }

    m_pausedContext = pausedContext;
    m_executionState = executionState;
    V8DebuggerAgentImpl::SkipPauseRequest result = agent->didPause(pausedContext, exception, breakpointIds, isPromiseRejection);
    if (result == V8DebuggerAgentImpl::RequestNoSkip) {
        m_runningNestedMessageLoop = true;
        int groupId = getGroupId(pausedContext);
        DCHECK(groupId);
        m_client->runMessageLoopOnPause(groupId);
        // The agent may have been removed in the nested loop.
        agent = findEnabledDebuggerAgent(pausedContext);
        if (agent)
            agent->didContinue();
        m_runningNestedMessageLoop = false;
    }
    m_pausedContext.Clear();
    m_executionState.Clear();

    if (result == V8DebuggerAgentImpl::RequestStepFrame) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod("stepFrameStatement", 1, argv);
    } else if (result == V8DebuggerAgentImpl::RequestStepInto) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    } else if (result == V8DebuggerAgentImpl::RequestStepOut) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod(stepOutV8MethodName, 1, argv);
    }
}

void V8DebuggerImpl::v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails)
{
    V8DebuggerImpl* thisPtr = toV8DebuggerImpl(eventDetails.GetCallbackData());
    thisPtr->handleV8DebugEvent(eventDetails);
}

v8::Local<v8::Value> V8DebuggerImpl::callInternalGetterFunction(v8::Local<v8::Object> object, const char* functionName)
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Value> getterValue = object->Get(v8InternalizedString(functionName));
    DCHECK(!getterValue.IsEmpty() && getterValue->IsFunction());
    return v8::Local<v8::Function>::Cast(getterValue)->Call(m_isolate->GetCurrentContext(), object, 0, 0).ToLocalChecked();
}

void V8DebuggerImpl::handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails)
{
    if (!enabled())
        return;
    v8::DebugEvent event = eventDetails.GetEvent();
    if (event != v8::AsyncTaskEvent && event != v8::Break && event != v8::Exception && event != v8::AfterCompile && event != v8::BeforeCompile && event != v8::CompileError)
        return;

    v8::Local<v8::Context> eventContext = eventDetails.GetEventContext();
    DCHECK(!eventContext.IsEmpty());

    V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(eventContext);
    if (agent) {
        v8::HandleScope scope(m_isolate);
        if (event == v8::AfterCompile || event == v8::CompileError) {
            v8::Context::Scope contextScope(debuggerContext());
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> value = callDebuggerMethod("getAfterCompileScript", 1, argv).ToLocalChecked();
            DCHECK(value->IsObject());
            v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
            agent->didParseSource(createParsedScript(object, event == v8::AfterCompile));
        } else if (event == v8::Exception) {
            v8::Local<v8::Object> eventData = eventDetails.GetEventData();
            v8::Local<v8::Value> exception = callInternalGetterFunction(eventData, "exception");
            v8::Local<v8::Value> promise = callInternalGetterFunction(eventData, "promise");
            bool isPromiseRejection = !promise.IsEmpty() && promise->IsObject();
            handleProgramBreak(eventContext, eventDetails.GetExecutionState(), exception, v8::Local<v8::Array>(), isPromiseRejection);
        } else if (event == v8::Break) {
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> hitBreakpoints = callDebuggerMethod("getBreakpointNumbers", 1, argv).ToLocalChecked();
            DCHECK(hitBreakpoints->IsArray());
            handleProgramBreak(eventContext, eventDetails.GetExecutionState(), v8::Local<v8::Value>(), hitBreakpoints.As<v8::Array>());
        } else if (event == v8::AsyncTaskEvent) {
            if (agent->v8AsyncTaskEventsEnabled())
                handleV8AsyncTaskEvent(agent, eventContext, eventDetails.GetExecutionState(), eventDetails.GetEventData());
        }
    }
}

void V8DebuggerImpl::handleV8AsyncTaskEvent(V8DebuggerAgentImpl* agent, v8::Local<v8::Context> context, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
{
    String16 type = toProtocolStringWithTypeCheck(callInternalGetterFunction(eventData, "type"));
    String16 name = toProtocolStringWithTypeCheck(callInternalGetterFunction(eventData, "name"));
    int id = callInternalGetterFunction(eventData, "id")->ToInteger(m_isolate)->Value();

    m_pausedContext = context;
    m_executionState = executionState;
    agent->didReceiveV8AsyncTaskEvent(context, type, name, id);
    m_pausedContext.Clear();
    m_executionState.Clear();
}

V8DebuggerParsedScript V8DebuggerImpl::createParsedScript(v8::Local<v8::Object> object, bool success)
{
    v8::Local<v8::Value> id = object->Get(v8InternalizedString("id"));
    DCHECK(!id.IsEmpty() && id->IsInt32());

    V8DebuggerParsedScript parsedScript;
    parsedScript.scriptId = String16::number(id->Int32Value());
    parsedScript.script.setURL(toProtocolStringWithTypeCheck(object->Get(v8InternalizedString("name"))))
        .setSourceURL(toProtocolStringWithTypeCheck(object->Get(v8InternalizedString("sourceURL"))))
        .setSourceMappingURL(toProtocolStringWithTypeCheck(object->Get(v8InternalizedString("sourceMappingURL"))))
        .setSource(toProtocolStringWithTypeCheck(object->Get(v8InternalizedString("source"))))
        .setStartLine(object->Get(v8InternalizedString("startLine"))->ToInteger(m_isolate)->Value())
        .setStartColumn(object->Get(v8InternalizedString("startColumn"))->ToInteger(m_isolate)->Value())
        .setEndLine(object->Get(v8InternalizedString("endLine"))->ToInteger(m_isolate)->Value())
        .setEndColumn(object->Get(v8InternalizedString("endColumn"))->ToInteger(m_isolate)->Value())
        .setIsContentScript(object->Get(v8InternalizedString("isContentScript"))->ToBoolean(m_isolate)->Value())
        .setIsInternalScript(object->Get(v8InternalizedString("isInternalScript"))->ToBoolean(m_isolate)->Value())
        .setExecutionContextId(object->Get(v8InternalizedString("executionContextId"))->ToInteger(m_isolate)->Value())
        .setIsLiveEdit(inLiveEditScope);
    parsedScript.success = success;
    return parsedScript;
}

void V8DebuggerImpl::compileDebuggerScript()
{
    if (!m_debuggerScript.IsEmpty()) {
        NOTREACHED();
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::String> scriptValue = v8::String::NewFromUtf8(m_isolate, DebuggerScript_js, v8::NewStringType::kInternalized, sizeof(DebuggerScript_js)).ToLocalChecked();
    v8::Local<v8::Value> value;
    if (!compileAndRunInternalScript(debuggerContext(), scriptValue).ToLocal(&value))
        return;
    DCHECK(value->IsObject());
    m_debuggerScript.Reset(m_isolate, value.As<v8::Object>());
}

v8::Local<v8::Context> V8DebuggerImpl::debuggerContext() const
{
    DCHECK(!m_debuggerContext.IsEmpty());
    return m_debuggerContext.Get(m_isolate);
}

v8::Local<v8::String> V8DebuggerImpl::v8InternalizedString(const char* str) const
{
    return v8::String::NewFromUtf8(m_isolate, str, v8::NewStringType::kInternalized).ToLocalChecked();
}

v8::MaybeLocal<v8::Value> V8DebuggerImpl::functionScopes(v8::Local<v8::Function> function)
{
    if (!enabled()) {
        NOTREACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { function };
    return callDebuggerMethod("getFunctionScopes", 1, argv);
}

v8::Local<v8::Value> V8DebuggerImpl::generatorObjectDetails(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        NOTREACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getGeneratorObjectDetails", 1, argv).ToLocalChecked();
}

v8::Local<v8::Value> V8DebuggerImpl::collectionEntries(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        NOTREACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getCollectionEntries", 1, argv).ToLocalChecked();
}

bool V8DebuggerImpl::isPaused()
{
    return !m_pausedContext.IsEmpty();
}

v8::MaybeLocal<v8::Value> V8DebuggerImpl::runCompiledScript(v8::Local<v8::Context> context, v8::Local<v8::Script> script)
{
    // TODO(dgozman): get rid of this check.
    if (!m_client->isExecutionAllowed())
        return v8::MaybeLocal<v8::Value>();

    v8::MicrotasksScope microtasksScope(m_isolate, v8::MicrotasksScope::kRunMicrotasks);
    int groupId = getGroupId(context);
    if (V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(groupId))
        agent->willExecuteScript(script->GetUnboundScript()->GetId());
    v8::MaybeLocal<v8::Value> result = script->Run(context);
    // Get agent from the map again, since it could have detached during script execution.
    if (V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(groupId))
        agent->didExecuteScript();
    return result;
}

v8::MaybeLocal<v8::Value> V8DebuggerImpl::callFunction(v8::Local<v8::Function> function, v8::Local<v8::Context> context, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    // TODO(dgozman): get rid of this check.
    if (!m_client->isExecutionAllowed())
        return v8::MaybeLocal<v8::Value>();

    v8::MicrotasksScope microtasksScope(m_isolate, v8::MicrotasksScope::kRunMicrotasks);
    int groupId = getGroupId(context);
    if (V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(groupId))
        agent->willExecuteScript(function->ScriptId());
    v8::MaybeLocal<v8::Value> result = function->Call(context, receiver, argc, info);
    // Get agent from the map again, since it could have detached during script execution.
    if (V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(groupId))
        agent->didExecuteScript();
    return result;
}

v8::MaybeLocal<v8::Value> V8DebuggerImpl::compileAndRunInternalScript(v8::Local<v8::Context> context, v8::Local<v8::String> source)
{
    v8::Local<v8::Script> script = compileInternalScript(context, source, String());
    if (script.IsEmpty())
        return v8::MaybeLocal<v8::Value>();
    v8::MicrotasksScope microtasksScope(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    return script->Run(context);
}

v8::Local<v8::Script> V8DebuggerImpl::compileInternalScript(v8::Local<v8::Context> context, v8::Local<v8::String> code, const String16& fileName)
{
    // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
    // 1, whereas v8 starts at 0.
    v8::ScriptOrigin origin(
        toV8String(m_isolate, fileName),
        v8::Integer::New(m_isolate, 0),
        v8::Integer::New(m_isolate, 0),
        v8::False(m_isolate), // sharable
        v8::Local<v8::Integer>(),
        v8::True(m_isolate), // internal
        toV8String(m_isolate, String16()), // sourceMap
        v8::True(m_isolate)); // opaqueresource
    v8::ScriptCompiler::Source source(code, origin);
    v8::Local<v8::Script> script;
    if (!v8::ScriptCompiler::Compile(context, &source, v8::ScriptCompiler::kNoCompileOptions).ToLocal(&script))
        return v8::Local<v8::Script>();
    return script;
}

std::unique_ptr<V8StackTrace> V8DebuggerImpl::createStackTrace(v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize)
{
    V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(m_isolate->GetCurrentContext());
    return V8StackTraceImpl::create(agent, stackTrace, maxStackSize);
}

std::unique_ptr<V8InspectorSession> V8DebuggerImpl::connect(int contextGroupId)
{
    DCHECK(!m_sessions.contains(contextGroupId));
    std::unique_ptr<V8InspectorSessionImpl> session = V8InspectorSessionImpl::create(this, contextGroupId);
    m_sessions.set(contextGroupId, session.get());
    return std::move(session);
}

void V8DebuggerImpl::disconnect(V8InspectorSessionImpl* session)
{
    DCHECK(m_sessions.contains(session->contextGroupId()));
    m_sessions.remove(session->contextGroupId());
}

void V8DebuggerImpl::contextCreated(const V8ContextInfo& info)
{
    DCHECK(info.context->GetIsolate() == m_isolate);
    // TODO(dgozman): make s_lastContextId non-static.
    int contextId = atomicIncrement(&s_lastContextId);
    String16 debugData = String16::number(info.contextGroupId) + "," + String16::number(contextId) + "," + (info.isDefault ? "default" : "nondefault");
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(info.context);
    info.context->SetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex), toV8String(m_isolate, debugData));

    if (!m_contexts.contains(info.contextGroupId))
        m_contexts.set(info.contextGroupId, wrapUnique(new ContextByIdMap()));
    DCHECK(!m_contexts.get(info.contextGroupId)->contains(contextId));

    std::unique_ptr<InspectedContext> contextOwner(new InspectedContext(this, info, contextId));
    InspectedContext* inspectedContext = contextOwner.get();
    m_contexts.get(info.contextGroupId)->set(contextId, std::move(contextOwner));

    if (V8InspectorSessionImpl* session = m_sessions.get(info.contextGroupId))
        session->runtimeAgentImpl()->reportExecutionContextCreated(inspectedContext);
}

void V8DebuggerImpl::contextDestroyed(v8::Local<v8::Context> context)
{
    int contextId = V8Debugger::contextId(context);
    int contextGroupId = getGroupId(context);
    if (!m_contexts.contains(contextGroupId) || !m_contexts.get(contextGroupId)->contains(contextId))
        return;

    InspectedContext* inspectedContext = m_contexts.get(contextGroupId)->get(contextId);
    if (V8InspectorSessionImpl* session = m_sessions.get(contextGroupId))
        session->runtimeAgentImpl()->reportExecutionContextDestroyed(inspectedContext);

    m_contexts.get(contextGroupId)->remove(contextId);
    if (m_contexts.get(contextGroupId)->isEmpty())
        m_contexts.remove(contextGroupId);
}

void V8DebuggerImpl::resetContextGroup(int contextGroupId)
{
    if (V8InspectorSessionImpl* session = m_sessions.get(contextGroupId))
        session->reset();
    m_contexts.remove(contextGroupId);
}

void V8DebuggerImpl::willExecuteScript(v8::Local<v8::Context> context, int scriptId)
{
    if (V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(context))
        agent->willExecuteScript(scriptId);
}

void V8DebuggerImpl::didExecuteScript(v8::Local<v8::Context> context)
{
    if (V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(context))
        agent->didExecuteScript();
}

void V8DebuggerImpl::idleStarted()
{
    m_isolate->GetCpuProfiler()->SetIdle(true);
}

void V8DebuggerImpl::idleFinished()
{
    m_isolate->GetCpuProfiler()->SetIdle(false);
}

std::unique_ptr<V8StackTrace> V8DebuggerImpl::captureStackTrace(size_t maxStackSize)
{
    V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(m_isolate->GetCurrentContext());
    return V8StackTraceImpl::capture(agent, maxStackSize);
}

v8::Local<v8::Context> V8DebuggerImpl::regexContext()
{
    if (m_regexContext.IsEmpty())
        m_regexContext.Reset(m_isolate, v8::Context::New(m_isolate));
    return m_regexContext.Get(m_isolate);
}

void V8DebuggerImpl::discardInspectedContext(int contextGroupId, int contextId)
{
    if (!m_contexts.contains(contextGroupId) || !m_contexts.get(contextGroupId)->contains(contextId))
        return;
    m_contexts.get(contextGroupId)->remove(contextId);
    if (m_contexts.get(contextGroupId)->isEmpty())
        m_contexts.remove(contextGroupId);
}

const V8DebuggerImpl::ContextByIdMap* V8DebuggerImpl::contextGroup(int contextGroupId)
{
    if (!m_contexts.contains(contextGroupId))
        return nullptr;
    return m_contexts.get(contextGroupId);
}

V8InspectorSessionImpl* V8DebuggerImpl::sessionForContextGroup(int contextGroupId)
{
    return contextGroupId ? m_sessions.get(contextGroupId) : nullptr;
}

} // namespace blink
