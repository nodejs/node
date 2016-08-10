// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8Debugger.h"

#include "platform/v8_inspector/DebuggerScript.h"
#include "platform/v8_inspector/ScriptBreakpoint.h"
#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8InspectorImpl.h"
#include "platform/v8_inspector/V8InternalValueType.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"

namespace blink {

namespace {
const char stepIntoV8MethodName[] = "stepIntoStatement";
const char stepOutV8MethodName[] = "stepOutOfFunction";
static const char v8AsyncTaskEventEnqueue[] = "enqueue";
static const char v8AsyncTaskEventWillHandle[] = "willHandle";
static const char v8AsyncTaskEventDidHandle[] = "didHandle";

inline v8::Local<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate)
{
    return value ? v8::True(isolate) : v8::False(isolate);
}

}

static bool inLiveEditScope = false;

v8::MaybeLocal<v8::Value> V8Debugger::callDebuggerMethod(const char* functionName, int argc, v8::Local<v8::Value> argv[])
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(debuggerScript->Get(toV8StringInternalized(m_isolate, functionName)));
    DCHECK(m_isolate->InContext());
    return function->Call(m_isolate->GetCurrentContext(), debuggerScript, argc, argv);
}

V8Debugger::V8Debugger(v8::Isolate* isolate, V8InspectorImpl* inspector)
    : m_isolate(isolate)
    , m_inspector(inspector)
    , m_lastContextId(0)
    , m_enableCount(0)
    , m_breakpointsActivated(true)
    , m_runningNestedMessageLoop(false)
    , m_ignoreScriptParsedEventsCounter(0)
    , m_maxAsyncCallStackDepth(0)
{
}

V8Debugger::~V8Debugger()
{
}

void V8Debugger::enable()
{
    if (m_enableCount++)
        return;
    DCHECK(!enabled());
    v8::HandleScope scope(m_isolate);
    v8::Debug::SetDebugEventListener(m_isolate, &V8Debugger::v8DebugEventCallback, v8::External::New(m_isolate, this));
    m_debuggerContext.Reset(m_isolate, v8::Debug::GetDebugContext(m_isolate));
    compileDebuggerScript();
}

void V8Debugger::disable()
{
    if (--m_enableCount)
        return;
    DCHECK(enabled());
    clearBreakpoints();
    m_debuggerScript.Reset();
    m_debuggerContext.Reset();
    allAsyncTasksCanceled();
    v8::Debug::SetDebugEventListener(m_isolate, nullptr);
}

bool V8Debugger::enabled() const
{
    return !m_debuggerScript.IsEmpty();
}

// static
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

// static
int V8Debugger::getGroupId(v8::Local<v8::Context> context)
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

void V8Debugger::getCompiledScripts(int contextGroupId, std::vector<std::unique_ptr<V8DebuggerScript>>& result)
{
    v8::HandleScope scope(m_isolate);
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
    DCHECK(!debuggerScript->IsUndefined());
    v8::Local<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(debuggerScript->Get(toV8StringInternalized(m_isolate, "getScripts")));
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

String16 V8Debugger::setBreakpoint(const String16& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber, bool interstatementLocation)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(toV8StringInternalized(m_isolate, "sourceID"), toV8String(m_isolate, sourceID));
    info->Set(toV8StringInternalized(m_isolate, "lineNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.lineNumber));
    info->Set(toV8StringInternalized(m_isolate, "columnNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.columnNumber));
    info->Set(toV8StringInternalized(m_isolate, "interstatementLocation"), v8Boolean(interstatementLocation, m_isolate));
    info->Set(toV8StringInternalized(m_isolate, "condition"), toV8String(m_isolate, scriptBreakpoint.condition));

    v8::Local<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(toV8StringInternalized(m_isolate, "setBreakpoint")));
    v8::Local<v8::Value> breakpointId = v8::Debug::Call(debuggerContext(), setBreakpointFunction, info).ToLocalChecked();
    if (!breakpointId->IsString())
        return "";
    *actualLineNumber = info->Get(toV8StringInternalized(m_isolate, "lineNumber"))->Int32Value();
    *actualColumnNumber = info->Get(toV8StringInternalized(m_isolate, "columnNumber"))->Int32Value();
    return toProtocolString(breakpointId.As<v8::String>());
}

void V8Debugger::removeBreakpoint(const String16& breakpointId)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(toV8StringInternalized(m_isolate, "breakpointId"), toV8String(m_isolate, breakpointId));

    v8::Local<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(toV8StringInternalized(m_isolate, "removeBreakpoint")));
    v8::Debug::Call(debuggerContext(), removeBreakpointFunction, info).ToLocalChecked();
}

void V8Debugger::clearBreakpoints()
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(toV8StringInternalized(m_isolate, "clearBreakpoints")));
    v8::Debug::Call(debuggerContext(), clearBreakpoints).ToLocalChecked();
}

void V8Debugger::setBreakpointsActivated(bool activated)
{
    if (!enabled()) {
        NOTREACHED();
        return;
    }
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(toV8StringInternalized(m_isolate, "enabled"), v8::Boolean::New(m_isolate, activated));
    v8::Local<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(toV8StringInternalized(m_isolate, "setBreakpointsActivated")));
    v8::Debug::Call(debuggerContext(), setBreakpointsActivated, info).ToLocalChecked();

    m_breakpointsActivated = activated;
}

V8Debugger::PauseOnExceptionsState V8Debugger::getPauseOnExceptionsState()
{
    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Undefined(m_isolate) };
    v8::Local<v8::Value> result = callDebuggerMethod("pauseOnExceptionsState", 0, argv).ToLocalChecked();
    return static_cast<V8Debugger::PauseOnExceptionsState>(result->Int32Value());
}

void V8Debugger::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Int32::New(m_isolate, pauseOnExceptionsState) };
    callDebuggerMethod("setPauseOnExceptionsState", 1, argv);
}

void V8Debugger::setPauseOnNextStatement(bool pause)
{
    if (m_runningNestedMessageLoop)
        return;
    if (pause)
        v8::Debug::DebugBreak(m_isolate);
    else
        v8::Debug::CancelDebugBreak(m_isolate);
}

bool V8Debugger::canBreakProgram()
{
    if (!m_breakpointsActivated)
        return false;
    return m_isolate->InContext();
}

void V8Debugger::breakProgram()
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
    if (!V8_FUNCTION_NEW_REMOVE_PROTOTYPE(m_isolate->GetCurrentContext(), &V8Debugger::breakProgramCallback, v8::External::New(m_isolate, this), 0).ToLocal(&breakFunction))
        return;
    v8::Debug::Call(debuggerContext(), breakFunction).ToLocalChecked();
}

void V8Debugger::continueProgram()
{
    if (isPaused())
        m_inspector->client()->quitMessageLoopOnPause();
    m_pausedContext.Clear();
    m_executionState.Clear();
}

void V8Debugger::stepIntoStatement()
{
    DCHECK(isPaused());
    DCHECK(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    continueProgram();
}

void V8Debugger::stepOverStatement()
{
    DCHECK(isPaused());
    DCHECK(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod("stepOverStatement", 1, argv);
    continueProgram();
}

void V8Debugger::stepOutOfFunction()
{
    DCHECK(isPaused());
    DCHECK(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepOutV8MethodName, 1, argv);
    continueProgram();
}

void V8Debugger::clearStepping()
{
    DCHECK(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Undefined(m_isolate) };
    callDebuggerMethod("clearStepping", 0, argv);
}

bool V8Debugger::setScriptSource(const String16& sourceID, v8::Local<v8::String> newSource, bool preview, ErrorString* error, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails, JavaScriptCallFrames* newCallFrames, Maybe<bool>* stackChanged)
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

JavaScriptCallFrames V8Debugger::currentCallFrames(int limit)
{
    if (!m_isolate->InContext())
        return JavaScriptCallFrames();
    v8::Local<v8::Value> currentCallFramesV8;
    if (m_executionState.IsEmpty()) {
        v8::Local<v8::Function> currentCallFramesFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(toV8StringInternalized(m_isolate, "currentCallFrames")));
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

static V8Debugger* toV8Debugger(v8::Local<v8::Value> data)
{
    void* p = v8::Local<v8::External>::Cast(data)->Value();
    return static_cast<V8Debugger*>(p);
}

void V8Debugger::breakProgramCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DCHECK_EQ(info.Length(), 2);
    V8Debugger* thisPtr = toV8Debugger(info.Data());
    if (!thisPtr->enabled())
        return;
    v8::Local<v8::Context> pausedContext = thisPtr->m_isolate->GetCurrentContext();
    v8::Local<v8::Value> exception;
    v8::Local<v8::Array> hitBreakpoints;
    thisPtr->handleProgramBreak(pausedContext, v8::Local<v8::Object>::Cast(info[0]), exception, hitBreakpoints);
}

void V8Debugger::handleProgramBreak(v8::Local<v8::Context> pausedContext, v8::Local<v8::Object> executionState, v8::Local<v8::Value> exception, v8::Local<v8::Array> hitBreakpointNumbers, bool isPromiseRejection)
{
    // Don't allow nested breaks.
    if (m_runningNestedMessageLoop)
        return;

    V8DebuggerAgentImpl* agent = m_inspector->enabledDebuggerAgentForGroup(getGroupId(pausedContext));
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
        m_inspector->client()->runMessageLoopOnPause(groupId);
        // The agent may have been removed in the nested loop.
        agent = m_inspector->enabledDebuggerAgentForGroup(getGroupId(pausedContext));
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

void V8Debugger::v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails)
{
    V8Debugger* thisPtr = toV8Debugger(eventDetails.GetCallbackData());
    thisPtr->handleV8DebugEvent(eventDetails);
}

v8::Local<v8::Value> V8Debugger::callInternalGetterFunction(v8::Local<v8::Object> object, const char* functionName)
{
    v8::MicrotasksScope microtasks(m_isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Value> getterValue = object->Get(toV8StringInternalized(m_isolate, functionName));
    DCHECK(!getterValue.IsEmpty() && getterValue->IsFunction());
    return v8::Local<v8::Function>::Cast(getterValue)->Call(m_isolate->GetCurrentContext(), object, 0, 0).ToLocalChecked();
}

void V8Debugger::handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails)
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

    V8DebuggerAgentImpl* agent = m_inspector->enabledDebuggerAgentForGroup(getGroupId(eventContext));
    if (agent) {
        v8::HandleScope scope(m_isolate);
        if (m_ignoreScriptParsedEventsCounter == 0 && (event == v8::AfterCompile || event == v8::CompileError)) {
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

void V8Debugger::handleV8AsyncTaskEvent(v8::Local<v8::Context> context, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
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

V8StackTraceImpl* V8Debugger::currentAsyncCallChain()
{
    if (!m_currentStacks.size())
        return nullptr;
    return m_currentStacks.back().get();
}

void V8Debugger::compileDebuggerScript()
{
    if (!m_debuggerScript.IsEmpty()) {
        NOTREACHED();
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::String> scriptValue = v8::String::NewFromUtf8(m_isolate, DebuggerScript_js, v8::NewStringType::kInternalized, sizeof(DebuggerScript_js)).ToLocalChecked();
    v8::Local<v8::Value> value;
    if (!m_inspector->compileAndRunInternalScript(debuggerContext(), scriptValue).ToLocal(&value)) {
        NOTREACHED();
        return;
    }
    DCHECK(value->IsObject());
    m_debuggerScript.Reset(m_isolate, value.As<v8::Object>());
}

v8::Local<v8::Context> V8Debugger::debuggerContext() const
{
    DCHECK(!m_debuggerContext.IsEmpty());
    return m_debuggerContext.Get(m_isolate);
}

v8::MaybeLocal<v8::Value> V8Debugger::functionScopes(v8::Local<v8::Function> function)
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

v8::MaybeLocal<v8::Array> V8Debugger::internalProperties(v8::Local<v8::Context> context, v8::Local<v8::Value> value)
{
    v8::Local<v8::Array> properties;
    if (!v8::Debug::GetInternalProperties(m_isolate, value).ToLocal(&properties))
        return v8::MaybeLocal<v8::Array>();
    if (value->IsFunction()) {
        v8::Local<v8::Function> function = value.As<v8::Function>();
        v8::Local<v8::Value> location = functionLocation(context, function);
        if (location->IsObject()) {
            properties->Set(properties->Length(), toV8StringInternalized(m_isolate, "[[FunctionLocation]]"));
            properties->Set(properties->Length(), location);
        }
        if (function->IsGeneratorFunction()) {
            properties->Set(properties->Length(), toV8StringInternalized(m_isolate, "[[IsGenerator]]"));
            properties->Set(properties->Length(), v8::True(m_isolate));
        }
    }
    if (!enabled())
        return properties;
    if (value->IsMap() || value->IsWeakMap() || value->IsSet() || value->IsWeakSet() || value->IsSetIterator() || value->IsMapIterator()) {
        v8::Local<v8::Value> entries = collectionEntries(context, v8::Local<v8::Object>::Cast(value));
        if (entries->IsArray()) {
            properties->Set(properties->Length(), toV8StringInternalized(m_isolate, "[[Entries]]"));
            properties->Set(properties->Length(), entries);
        }
    }
    if (value->IsGeneratorObject()) {
        v8::Local<v8::Value> location = generatorObjectLocation(v8::Local<v8::Object>::Cast(value));
        if (location->IsObject()) {
            properties->Set(properties->Length(), toV8StringInternalized(m_isolate, "[[GeneratorLocation]]"));
            properties->Set(properties->Length(), location);
        }
    }
    if (value->IsFunction()) {
        v8::Local<v8::Function> function = value.As<v8::Function>();
        v8::Local<v8::Value> boundFunction = function->GetBoundFunction();
        v8::Local<v8::Value> scopes;
        if (boundFunction->IsUndefined() && functionScopes(function).ToLocal(&scopes)) {
            properties->Set(properties->Length(), toV8StringInternalized(m_isolate, "[[Scopes]]"));
            properties->Set(properties->Length(), scopes);
        }
    }
    return properties;
}

v8::Local<v8::Value> V8Debugger::collectionEntries(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
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

v8::Local<v8::Value> V8Debugger::generatorObjectLocation(v8::Local<v8::Object> object)
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

v8::Local<v8::Value> V8Debugger::functionLocation(v8::Local<v8::Context> context, v8::Local<v8::Function> function)
{
    int scriptId = function->ScriptId();
    if (scriptId == v8::UnboundScript::kNoScriptId)
        return v8::Null(m_isolate);
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();
    if (lineNumber == v8::Function::kLineOffsetNotFound || columnNumber == v8::Function::kLineOffsetNotFound)
        return v8::Null(m_isolate);
    v8::Local<v8::Object> location = v8::Object::New(m_isolate);
    if (!location->Set(context, toV8StringInternalized(m_isolate, "scriptId"), toV8String(m_isolate, String16::fromInteger(scriptId))).FromMaybe(false))
        return v8::Null(m_isolate);
    if (!location->Set(context, toV8StringInternalized(m_isolate, "lineNumber"), v8::Integer::New(m_isolate, lineNumber)).FromMaybe(false))
        return v8::Null(m_isolate);
    if (!location->Set(context, toV8StringInternalized(m_isolate, "columnNumber"), v8::Integer::New(m_isolate, columnNumber)).FromMaybe(false))
        return v8::Null(m_isolate);
    if (!markAsInternal(context, location, V8InternalValueType::kLocation))
        return v8::Null(m_isolate);
    return location;
}

bool V8Debugger::isPaused()
{
    return !m_pausedContext.IsEmpty();
}

std::unique_ptr<V8StackTraceImpl> V8Debugger::createStackTrace(v8::Local<v8::StackTrace> stackTrace)
{
    int contextGroupId = m_isolate->InContext() ? getGroupId(m_isolate->GetCurrentContext()) : 0;
    return V8StackTraceImpl::create(this, contextGroupId, stackTrace, V8StackTraceImpl::maxCallStackSizeToCapture);
}

int V8Debugger::markContext(const V8ContextInfo& info)
{
    DCHECK(info.context->GetIsolate() == m_isolate);
    int contextId = ++m_lastContextId;
    String16 debugData = String16::fromInteger(info.contextGroupId) + "," + String16::fromInteger(contextId) + "," + info.auxData;
    v8::Context::Scope contextScope(info.context);
    info.context->SetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex), toV8String(m_isolate, debugData));
    return contextId;
}

void V8Debugger::setAsyncCallStackDepth(V8DebuggerAgentImpl* agent, int depth)
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
    m_maxAsyncCallStackDepth = maxAsyncCallStackDepth;
    if (!maxAsyncCallStackDepth)
        allAsyncTasksCanceled();
}

void V8Debugger::asyncTaskScheduled(const String16& taskName, void* task, bool recurring)
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

void V8Debugger::asyncTaskCanceled(void* task)
{
    if (!m_maxAsyncCallStackDepth)
        return;
    m_asyncTaskStacks.erase(task);
    m_recurringTasks.erase(task);
}

void V8Debugger::asyncTaskStarted(void* task)
{
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

void V8Debugger::asyncTaskFinished(void* task)
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

void V8Debugger::allAsyncTasksCanceled()
{
    m_asyncTaskStacks.clear();
    m_recurringTasks.clear();
    m_currentStacks.clear();
    m_currentTasks.clear();
}

void V8Debugger::muteScriptParsedEvents()
{
    ++m_ignoreScriptParsedEventsCounter;
}

void V8Debugger::unmuteScriptParsedEvents()
{
    --m_ignoreScriptParsedEventsCounter;
    DCHECK_GE(m_ignoreScriptParsedEventsCounter, 0);
}

std::unique_ptr<V8StackTraceImpl> V8Debugger::captureStackTrace(bool fullStack)
{
    if (!m_isolate->InContext())
        return nullptr;

    v8::HandleScope handles(m_isolate);
    int contextGroupId = getGroupId(m_isolate->GetCurrentContext());
    if (!contextGroupId)
        return nullptr;

    size_t stackSize = fullStack ? V8StackTraceImpl::maxCallStackSizeToCapture : 1;
    if (m_inspector->enabledRuntimeAgentForGroup(contextGroupId))
        stackSize = V8StackTraceImpl::maxCallStackSizeToCapture;

    return V8StackTraceImpl::capture(this, contextGroupId, stackSize);
}

} // namespace blink
