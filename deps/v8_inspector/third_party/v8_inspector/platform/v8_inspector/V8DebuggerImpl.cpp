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
#include "platform/v8_inspector/V8ConsoleAgentImpl.h"
#include "platform/v8_inspector/V8ConsoleMessage.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8InjectedScriptHost.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8InternalValueType.h"
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
static const char v8AsyncTaskEventEnqueue[] = "enqueue";
static const char v8AsyncTaskEventWillHandle[] = "willHandle";
static const char v8AsyncTaskEventDidHandle[] = "didHandle";

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
    , m_capturingStackTracesCount(0)
    , m_lastExceptionId(0)
    , m_enabledAgentsCount(0)
    , m_breakpointsActivated(true)
    , m_runningNestedMessageLoop(false)
    , m_maxAsyncCallStackDepth(0)
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
    allAsyncTasksCanceled();
    v8::Debug::SetDebugEventListener(m_isolate, nullptr);
}

bool V8DebuggerImpl::enabled() const
{
    return !m_debuggerScript.IsEmpty();
}

// static
int V8DebuggerImpl::contextId(v8::Local<v8::Context> context)
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

// static
int V8DebuggerImpl::getGroupId(v8::Local<v8::Context> context)
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
    SessionMap::iterator it = m_sessions.find(contextGroupId);
    if (it == m_sessions.end())
        return nullptr;
    V8DebuggerAgentImpl* agent = it->second->debuggerAgent();
    if (!agent->enabled())
        return nullptr;
    return agent;
}

V8DebuggerAgentImpl* V8DebuggerImpl::findEnabledDebuggerAgent(v8::Local<v8::Context> context)
{
    return findEnabledDebuggerAgent(getGroupId(context));
}

void V8DebuggerImpl::getCompiledScripts(int contextGroupId, std::vector<std::unique_ptr<V8DebuggerScript>>& result)
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
    result.reserve(scriptsArray->Length());
    for (unsigned i = 0; i < scriptsArray->Length(); ++i) {
        v8::Local<v8::Object> scriptObject = v8::Local<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(m_isolate, i)));
        result.push_back(wrapUnique(new V8DebuggerScript(m_isolate, scriptObject, inLiveEditScope)));
    }
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
    if (!v8::Function::New(m_isolate->GetCurrentContext(), &V8DebuggerImpl::breakProgramCallback, v8::External::New(m_isolate, this), 0, v8::ConstructorBehavior::kThrow).ToLocal(&breakFunction))
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

bool V8DebuggerImpl::setScriptSource(const String16& sourceID, v8::Local<v8::String> newSource, bool preview, ErrorString* error, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails, JavaScriptCallFrames* newCallFrames, Maybe<bool>* stackChanged)
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

    v8::Local<v8::Value> argv[] = { toV8String(m_isolate, sourceID), newSource, v8Boolean(preview, m_isolate) };

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
            if (!preview && isPaused()) {
                JavaScriptCallFrames frames = currentCallFrames();
                newCallFrames->swap(frames);
            }
            return true;
        }
    // Compile error.
    case 1:
        {
            *exceptionDetails = protocol::Runtime::ExceptionDetails::create()
                .setText(toProtocolStringWithTypeCheck(resultTuple->Get(2)))
                .setScriptId(String16("0"))
                .setLineNumber(resultTuple->Get(3)->ToInteger(m_isolate)->Value() - 1)
                .setColumnNumber(resultTuple->Get(4)->ToInteger(m_isolate)->Value() - 1).build();
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
        callFrames.push_back(JavaScriptCallFrame::create(debuggerContext(), v8::Local<v8::Object>::Cast(callFrameObject)));
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
    DCHECK_EQ(info.Length(), 2);
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

    std::vector<String16> breakpointIds;
    if (!hitBreakpointNumbers.IsEmpty()) {
        breakpointIds.reserve(hitBreakpointNumbers->Length());
        for (size_t i = 0; i < hitBreakpointNumbers->Length(); i++) {
            v8::Local<v8::Value> hitBreakpointNumber = hitBreakpointNumbers->Get(i);
            DCHECK(!hitBreakpointNumber.IsEmpty() && hitBreakpointNumber->IsInt32());
            breakpointIds.push_back(String16::fromInteger(hitBreakpointNumber->Int32Value()));
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

    if (event == v8::AsyncTaskEvent) {
        v8::HandleScope scope(m_isolate);
        handleV8AsyncTaskEvent(eventContext, eventDetails.GetExecutionState(), eventDetails.GetEventData());
        return;
    }

    V8DebuggerAgentImpl* agent = findEnabledDebuggerAgent(eventContext);
    if (agent) {
        v8::HandleScope scope(m_isolate);
        if (event == v8::AfterCompile || event == v8::CompileError) {
            v8::Context::Scope contextScope(debuggerContext());
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> value = callDebuggerMethod("getAfterCompileScript", 1, argv).ToLocalChecked();
            DCHECK(value->IsObject());
            v8::Local<v8::Object> scriptObject = v8::Local<v8::Object>::Cast(value);
            agent->didParseSource(wrapUnique(new V8DebuggerScript(m_isolate, scriptObject, inLiveEditScope)), event == v8::AfterCompile);
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
        }
    }
}

void V8DebuggerImpl::handleV8AsyncTaskEvent(v8::Local<v8::Context> context, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
{
    if (!m_maxAsyncCallStackDepth)
        return;

    String16 type = toProtocolStringWithTypeCheck(callInternalGetterFunction(eventData, "type"));
    String16 name = toProtocolStringWithTypeCheck(callInternalGetterFunction(eventData, "name"));
    int id = callInternalGetterFunction(eventData, "id")->ToInteger(m_isolate)->Value();
    // The scopes for the ids are defined by the eventData.name namespaces. There are currently two namespaces: "Object." and "Promise.".
    void* ptr = reinterpret_cast<void*>(id * 4 + (name[0] == 'P' ? 2 : 0) + 1);
    if (type == v8AsyncTaskEventEnqueue)
        asyncTaskScheduled(name, ptr, false);
    else if (type == v8AsyncTaskEventWillHandle)
        asyncTaskStarted(ptr);
    else if (type == v8AsyncTaskEventDidHandle)
        asyncTaskFinished(ptr);
    else
        NOTREACHED();
}

V8StackTraceImpl* V8DebuggerImpl::currentAsyncCallChain()
{
    if (!m_currentStacks.size())
        return nullptr;
    return m_currentStacks.back().get();
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
    v8::Local<v8::Value> scopesValue;
    if (!callDebuggerMethod("getFunctionScopes", 1, argv).ToLocal(&scopesValue) || !scopesValue->IsArray())
        return v8::MaybeLocal<v8::Value>();
    v8::Local<v8::Array> scopes = scopesValue.As<v8::Array>();
    v8::Local<v8::Context> context = m_debuggerContext.Get(m_isolate);
    if (!markAsInternal(context, scopes, V8InternalValueType::kScopeList))
        return v8::MaybeLocal<v8::Value>();
    if (!markArrayEntriesAsInternal(context, scopes, V8InternalValueType::kScope))
        return v8::MaybeLocal<v8::Value>();
    if (!scopes->SetPrototype(context, v8::Null(m_isolate)).FromMaybe(false))
        return v8::Undefined(m_isolate);
    return scopes;
}

v8::MaybeLocal<v8::Array> V8DebuggerImpl::internalProperties(v8::Local<v8::Context> context, v8::Local<v8::Value> value)
{
    v8::Local<v8::Array> properties;
    if (!v8::Debug::GetInternalProperties(m_isolate, value).ToLocal(&properties))
        return v8::MaybeLocal<v8::Array>();
    if (value->IsFunction()) {
        v8::Local<v8::Function> function = value.As<v8::Function>();
        v8::Local<v8::Value> location = functionLocation(context, function);
        if (location->IsObject()) {
            properties->Set(properties->Length(), v8InternalizedString("[[FunctionLocation]]"));
            properties->Set(properties->Length(), location);
        }
        if (function->IsGeneratorFunction()) {
            properties->Set(properties->Length(), v8InternalizedString("[[IsGenerator]]"));
            properties->Set(properties->Length(), v8::True(m_isolate));
        }
    }
    if (!enabled())
        return properties;
    if (value->IsMap() || value->IsWeakMap() || value->IsSet() || value->IsWeakSet() || value->IsSetIterator() || value->IsMapIterator()) {
        v8::Local<v8::Value> entries = collectionEntries(context, v8::Local<v8::Object>::Cast(value));
        if (entries->IsArray()) {
            properties->Set(properties->Length(), v8InternalizedString("[[Entries]]"));
            properties->Set(properties->Length(), entries);
        }
    }
    if (value->IsGeneratorObject()) {
        v8::Local<v8::Value> location = generatorObjectLocation(v8::Local<v8::Object>::Cast(value));
        if (location->IsObject()) {
            properties->Set(properties->Length(), v8InternalizedString("[[GeneratorLocation]]"));
            properties->Set(properties->Length(), location);
        }
    }
    if (value->IsFunction()) {
        v8::Local<v8::Function> function = value.As<v8::Function>();
        v8::Local<v8::Value> boundFunction = function->GetBoundFunction();
        v8::Local<v8::Value> scopes;
        if (boundFunction->IsUndefined() && functionScopes(function).ToLocal(&scopes)) {
            properties->Set(properties->Length(), v8InternalizedString("[[Scopes]]"));
            properties->Set(properties->Length(), scopes);
        }
    }
    return properties;
}

v8::Local<v8::Value> V8DebuggerImpl::collectionEntries(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    if (!enabled()) {
        NOTREACHED();
        return v8::Undefined(m_isolate);
    }
    v8::Local<v8::Value> argv[] = { object };
    v8::Local<v8::Value> entriesValue = callDebuggerMethod("getCollectionEntries", 1, argv).ToLocalChecked();
    if (!entriesValue->IsArray())
        return v8::Undefined(m_isolate);
    v8::Local<v8::Array> entries = entriesValue.As<v8::Array>();
    if (!markArrayEntriesAsInternal(context, entries, V8InternalValueType::kEntry))
        return v8::Undefined(m_isolate);
    if (!entries->SetPrototype(context, v8::Null(m_isolate)).FromMaybe(false))
        return v8::Undefined(m_isolate);
    return entries;
}

v8::Local<v8::Value> V8DebuggerImpl::generatorObjectLocation(v8::Local<v8::Object> object)
{
    if (!enabled()) {
        NOTREACHED();
        return v8::Null(m_isolate);
    }
    v8::Local<v8::Value> argv[] = { object };
    v8::Local<v8::Value> location = callDebuggerMethod("getGeneratorObjectLocation", 1, argv).ToLocalChecked();
    if (!location->IsObject())
        return v8::Null(m_isolate);
    v8::Local<v8::Context> context = m_debuggerContext.Get(m_isolate);
    if (!markAsInternal(context, v8::Local<v8::Object>::Cast(location), V8InternalValueType::kLocation))
        return v8::Null(m_isolate);
    return location;
}

v8::Local<v8::Value> V8DebuggerImpl::functionLocation(v8::Local<v8::Context> context, v8::Local<v8::Function> function)
{
    int scriptId = function->ScriptId();
    if (scriptId == v8::UnboundScript::kNoScriptId)
        return v8::Null(m_isolate);
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();
    if (lineNumber == v8::Function::kLineOffsetNotFound || columnNumber == v8::Function::kLineOffsetNotFound)
        return v8::Null(m_isolate);
    v8::Local<v8::Object> location = v8::Object::New(m_isolate);
    if (!location->Set(context, v8InternalizedString("scriptId"), toV8String(m_isolate, String16::fromInteger(scriptId))).FromMaybe(false))
        return v8::Null(m_isolate);
    if (!location->Set(context, v8InternalizedString("lineNumber"), v8::Integer::New(m_isolate, lineNumber)).FromMaybe(false))
        return v8::Null(m_isolate);
    if (!location->Set(context, v8InternalizedString("columnNumber"), v8::Integer::New(m_isolate, columnNumber)).FromMaybe(false))
        return v8::Null(m_isolate);
    if (!markAsInternal(context, location, V8InternalValueType::kLocation))
        return v8::Null(m_isolate);
    return location;
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

void V8DebuggerImpl::enableStackCapturingIfNeeded()
{
    if (!m_capturingStackTracesCount)
        V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(m_isolate, true);
    ++m_capturingStackTracesCount;
}

void V8DebuggerImpl::disableStackCapturingIfNeeded()
{
    if (!(--m_capturingStackTracesCount))
        V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(m_isolate, false);
}

V8ConsoleMessageStorage* V8DebuggerImpl::ensureConsoleMessageStorage(int contextGroupId)
{
    ConsoleStorageMap::iterator storageIt = m_consoleStorageMap.find(contextGroupId);
    if (storageIt == m_consoleStorageMap.end())
        storageIt = m_consoleStorageMap.insert(std::make_pair(contextGroupId, wrapUnique(new V8ConsoleMessageStorage(this, contextGroupId)))).first;
    return storageIt->second.get();
}

std::unique_ptr<V8StackTrace> V8DebuggerImpl::createStackTrace(v8::Local<v8::StackTrace> stackTrace)
{
    int contextGroupId = m_isolate->InContext() ? getGroupId(m_isolate->GetCurrentContext()) : 0;
    return V8StackTraceImpl::create(this, contextGroupId, stackTrace, V8StackTraceImpl::maxCallStackSizeToCapture);
}

std::unique_ptr<V8InspectorSession> V8DebuggerImpl::connect(int contextGroupId, protocol::FrontendChannel* channel, V8InspectorSessionClient* client, const String16* state)
{
    DCHECK(m_sessions.find(contextGroupId) == m_sessions.cend());
    std::unique_ptr<V8InspectorSessionImpl> session =
        V8InspectorSessionImpl::create(this, contextGroupId, channel, client, state);
    m_sessions[contextGroupId] = session.get();
    return std::move(session);
}

void V8DebuggerImpl::disconnect(V8InspectorSessionImpl* session)
{
    DCHECK(m_sessions.find(session->contextGroupId()) != m_sessions.end());
    m_sessions.erase(session->contextGroupId());
}

InspectedContext* V8DebuggerImpl::getContext(int groupId, int contextId) const
{
    ContextsByGroupMap::const_iterator contextGroupIt = m_contexts.find(groupId);
    if (contextGroupIt == m_contexts.end())
        return nullptr;

    ContextByIdMap::iterator contextIt = contextGroupIt->second->find(contextId);
    if (contextIt == contextGroupIt->second->end())
        return nullptr;

    return contextIt->second.get();
}

void V8DebuggerImpl::contextCreated(const V8ContextInfo& info)
{
    DCHECK(info.context->GetIsolate() == m_isolate);
    // TODO(dgozman): make s_lastContextId non-static.
    int contextId = atomicIncrement(&s_lastContextId);
    String16 debugData = String16::fromInteger(info.contextGroupId) + "," + String16::fromInteger(contextId) + "," + (info.isDefault ? "default" : "nondefault");
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(info.context);
    info.context->SetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex), toV8String(m_isolate, debugData));

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

void V8DebuggerImpl::contextDestroyed(v8::Local<v8::Context> context)
{
    int contextId = V8DebuggerImpl::contextId(context);
    int contextGroupId = getGroupId(context);

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

void V8DebuggerImpl::resetContextGroup(int contextGroupId)
{
    m_consoleStorageMap.erase(contextGroupId);
    SessionMap::iterator session = m_sessions.find(contextGroupId);
    if (session != m_sessions.end())
        session->second->reset();
    m_contexts.erase(contextGroupId);
}

void V8DebuggerImpl::setAsyncCallStackDepth(V8DebuggerAgentImpl* agent, int depth)
{
    if (depth <= 0)
        m_maxAsyncCallStackDepthMap.erase(agent);
    else
        m_maxAsyncCallStackDepthMap[agent] = depth;

    int maxAsyncCallStackDepth = 0;
    for (const auto& pair : m_maxAsyncCallStackDepthMap) {
        if (pair.second > maxAsyncCallStackDepth)
            maxAsyncCallStackDepth = pair.second;
    }

    if (m_maxAsyncCallStackDepth == maxAsyncCallStackDepth)
        return;

    if (maxAsyncCallStackDepth && !m_maxAsyncCallStackDepth)
        m_client->enableAsyncInstrumentation();
    else if (!maxAsyncCallStackDepth && m_maxAsyncCallStackDepth)
        m_client->disableAsyncInstrumentation();

    m_maxAsyncCallStackDepth = maxAsyncCallStackDepth;
    if (!maxAsyncCallStackDepth)
        allAsyncTasksCanceled();
}

void V8DebuggerImpl::asyncTaskScheduled(const String16& taskName, void* task, bool recurring)
{
    if (!m_maxAsyncCallStackDepth)
        return;
    v8::HandleScope scope(m_isolate);
    int contextGroupId = m_isolate->InContext() ? getGroupId(m_isolate->GetCurrentContext()) : 0;
    std::unique_ptr<V8StackTraceImpl> chain = V8StackTraceImpl::capture(this, contextGroupId, V8StackTraceImpl::maxCallStackSizeToCapture, taskName);
    if (chain) {
        m_asyncTaskStacks[task] = std::move(chain);
        if (recurring)
            m_recurringTasks.insert(task);
    }
}

void V8DebuggerImpl::asyncTaskCanceled(void* task)
{
    if (!m_maxAsyncCallStackDepth)
        return;
    m_asyncTaskStacks.erase(task);
    m_recurringTasks.erase(task);
}

void V8DebuggerImpl::asyncTaskStarted(void* task)
{
    // Not enabled, return.
    if (!m_maxAsyncCallStackDepth)
        return;

    m_currentTasks.push_back(task);
    AsyncTaskToStackTrace::iterator stackIt = m_asyncTaskStacks.find(task);
    // Needs to support following order of events:
    // - asyncTaskScheduled
    //   <-- attached here -->
    // - asyncTaskStarted
    // - asyncTaskCanceled <-- canceled before finished
    //   <-- async stack requested here -->
    // - asyncTaskFinished
    std::unique_ptr<V8StackTraceImpl> stack;
    if (stackIt != m_asyncTaskStacks.end() && stackIt->second)
        stack = stackIt->second->cloneImpl();
    m_currentStacks.push_back(std::move(stack));
}

void V8DebuggerImpl::asyncTaskFinished(void* task)
{
    if (!m_maxAsyncCallStackDepth)
        return;
    // We could start instrumenting half way and the stack is empty.
    if (!m_currentStacks.size())
        return;

    DCHECK(m_currentTasks.back() == task);
    m_currentTasks.pop_back();

    m_currentStacks.pop_back();
    if (m_recurringTasks.find(task) == m_recurringTasks.end())
        m_asyncTaskStacks.erase(task);
}

void V8DebuggerImpl::allAsyncTasksCanceled()
{
    m_asyncTaskStacks.clear();
    m_recurringTasks.clear();
    m_currentStacks.clear();
    m_currentTasks.clear();
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

void V8DebuggerImpl::logToConsole(v8::Local<v8::Context> context, v8::Local<v8::Value> arg1, v8::Local<v8::Value> arg2)
{
    int contextGroupId = getGroupId(context);
    InspectedContext* inspectedContext = getContext(contextGroupId, contextId(context));
    if (!inspectedContext)
        return;
    std::vector<v8::Local<v8::Value>> arguments;
    if (!arg1.IsEmpty())
        arguments.push_back(arg1);
    if (!arg2.IsEmpty())
        arguments.push_back(arg2);
    ensureConsoleMessageStorage(contextGroupId)->addMessage(V8ConsoleMessage::createForConsoleAPI(m_client->currentTimeMS(), ConsoleAPIType::kLog, arguments, captureStackTrace(false), inspectedContext));
}

void V8DebuggerImpl::exceptionThrown(int contextGroupId, const String16& errorMessage, const String16& url, unsigned lineNumber, unsigned columnNumber, std::unique_ptr<V8StackTrace> stackTrace, int scriptId)
{
    unsigned exceptionId = ++m_lastExceptionId;
    std::unique_ptr<V8ConsoleMessage> consoleMessage = V8ConsoleMessage::createForException(m_client->currentTimeMS(), errorMessage, url, lineNumber, columnNumber, std::move(stackTrace), scriptId, m_isolate, 0, v8::Local<v8::Value>(), exceptionId);
    ensureConsoleMessageStorage(contextGroupId)->addMessage(std::move(consoleMessage));
}

unsigned V8DebuggerImpl::promiseRejected(v8::Local<v8::Context> context, const String16& errorMessage, v8::Local<v8::Value> exception, const String16& url, unsigned lineNumber, unsigned columnNumber, std::unique_ptr<V8StackTrace> stackTrace, int scriptId)
{
    int contextGroupId = getGroupId(context);
    if (!contextGroupId)
        return 0;
    unsigned exceptionId = ++m_lastExceptionId;
    std::unique_ptr<V8ConsoleMessage> consoleMessage = V8ConsoleMessage::createForException(m_client->currentTimeMS(), errorMessage, url, lineNumber, columnNumber, std::move(stackTrace), scriptId, m_isolate, contextId(context), exception, exceptionId);
    ensureConsoleMessageStorage(contextGroupId)->addMessage(std::move(consoleMessage));
    return exceptionId;
}

void V8DebuggerImpl::promiseRejectionRevoked(v8::Local<v8::Context> context, unsigned promiseRejectionId)
{
    int contextGroupId = getGroupId(context);
    if (!contextGroupId)
        return;

    std::unique_ptr<V8ConsoleMessage> consoleMessage = V8ConsoleMessage::createForRevokedException(m_client->currentTimeMS(), "Handler added to rejected promise", promiseRejectionId);
    ensureConsoleMessageStorage(contextGroupId)->addMessage(std::move(consoleMessage));
}

std::unique_ptr<V8StackTrace> V8DebuggerImpl::captureStackTrace(bool fullStack)
{
    if (!m_isolate->InContext())
        return nullptr;

    v8::HandleScope handles(m_isolate);
    int contextGroupId = getGroupId(m_isolate->GetCurrentContext());
    if (!contextGroupId)
        return nullptr;

    size_t stackSize = fullStack ? V8StackTraceImpl::maxCallStackSizeToCapture : 1;
    SessionMap::iterator sessionIt = m_sessions.find(contextGroupId);
    if (sessionIt != m_sessions.end() && sessionIt->second->runtimeAgent()->enabled())
        stackSize = V8StackTraceImpl::maxCallStackSizeToCapture;

    return V8StackTraceImpl::capture(this, contextGroupId, stackSize);
}

v8::Local<v8::Context> V8DebuggerImpl::regexContext()
{
    if (m_regexContext.IsEmpty())
        m_regexContext.Reset(m_isolate, v8::Context::New(m_isolate));
    return m_regexContext.Get(m_isolate);
}

void V8DebuggerImpl::discardInspectedContext(int contextGroupId, int contextId)
{
    if (!getContext(contextGroupId, contextId))
        return;
    m_contexts[contextGroupId]->erase(contextId);
    if (m_contexts[contextGroupId]->empty())
        m_contexts.erase(contextGroupId);
}

const V8DebuggerImpl::ContextByIdMap* V8DebuggerImpl::contextGroup(int contextGroupId)
{
    ContextsByGroupMap::iterator iter = m_contexts.find(contextGroupId);
    return iter == m_contexts.end() ? nullptr : iter->second.get();
}

V8InspectorSessionImpl* V8DebuggerImpl::sessionForContextGroup(int contextGroupId)
{
    if (!contextGroupId)
        return nullptr;
    SessionMap::iterator iter = m_sessions.find(contextGroupId);
    return iter == m_sessions.end() ? nullptr : iter->second;
}

} // namespace blink
