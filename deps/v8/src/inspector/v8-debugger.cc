// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger.h"

#include "src/inspector/debugger-script.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/script-breakpoint.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-internal-value-type.h"
#include "src/inspector/v8-stack-trace-impl.h"
#include "src/inspector/v8-value-copier.h"

#include "include/v8-util.h"

namespace v8_inspector {

namespace {

// Based on DevTools frontend measurement, with asyncCallStackDepth = 4,
// average async call stack tail requires ~1 Kb. Let's reserve ~ 128 Mb
// for async stacks.
static const int kMaxAsyncTaskStacks = 128 * 1024;

inline v8::Local<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate) {
  return value ? v8::True(isolate) : v8::False(isolate);
}

}  // namespace

static bool inLiveEditScope = false;

v8::MaybeLocal<v8::Value> V8Debugger::callDebuggerMethod(
    const char* functionName, int argc, v8::Local<v8::Value> argv[],
    bool catchExceptions) {
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  DCHECK(m_isolate->InContext());
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      debuggerScript
          ->Get(context, toV8StringInternalized(m_isolate, functionName))
          .ToLocalChecked());
  if (catchExceptions) {
    v8::TryCatch try_catch(m_isolate);
    return function->Call(context, debuggerScript, argc, argv);
  }
  return function->Call(context, debuggerScript, argc, argv);
}

V8Debugger::V8Debugger(v8::Isolate* isolate, V8InspectorImpl* inspector)
    : m_isolate(isolate),
      m_inspector(inspector),
      m_enableCount(0),
      m_breakpointsActivated(true),
      m_runningNestedMessageLoop(false),
      m_ignoreScriptParsedEventsCounter(0),
      m_maxAsyncCallStacks(kMaxAsyncTaskStacks),
      m_lastTaskId(0),
      m_maxAsyncCallStackDepth(0),
      m_pauseOnExceptionsState(v8::debug::NoBreakOnException),
      m_wasmTranslation(isolate) {}

V8Debugger::~V8Debugger() {}

void V8Debugger::enable() {
  if (m_enableCount++) return;
  DCHECK(!enabled());
  v8::HandleScope scope(m_isolate);
  v8::debug::SetDebugEventListener(m_isolate, &V8Debugger::v8DebugEventCallback,
                                   v8::External::New(m_isolate, this));
  v8::debug::SetAsyncTaskListener(m_isolate, &V8Debugger::v8AsyncTaskListener,
                                  this);
  m_debuggerContext.Reset(m_isolate, v8::debug::GetDebugContext(m_isolate));
  v8::debug::ChangeBreakOnException(m_isolate, v8::debug::NoBreakOnException);
  m_pauseOnExceptionsState = v8::debug::NoBreakOnException;
  compileDebuggerScript();
}

void V8Debugger::disable() {
  if (--m_enableCount) return;
  DCHECK(enabled());
  clearBreakpoints();
  m_debuggerScript.Reset();
  m_debuggerContext.Reset();
  allAsyncTasksCanceled();
  m_wasmTranslation.Clear();
  v8::debug::SetDebugEventListener(m_isolate, nullptr);
  v8::debug::SetAsyncTaskListener(m_isolate, nullptr, nullptr);
}

bool V8Debugger::enabled() const { return !m_debuggerScript.IsEmpty(); }

void V8Debugger::getCompiledScripts(
    int contextGroupId,
    std::vector<std::unique_ptr<V8DebuggerScript>>& result) {
  v8::HandleScope scope(m_isolate);
  v8::PersistentValueVector<v8::debug::Script> scripts(m_isolate);
  v8::debug::GetLoadedScripts(m_isolate, scripts);
  for (size_t i = 0; i < scripts.Size(); ++i) {
    v8::Local<v8::debug::Script> script = scripts.Get(i);
    if (!script->WasCompiled()) continue;
    v8::Local<v8::Value> contextData;
    if (!script->ContextData().ToLocal(&contextData) || !contextData->IsInt32())
      continue;
    int contextId = static_cast<int>(contextData.As<v8::Int32>()->Value());
    if (m_inspector->contextGroupId(contextId) != contextGroupId) continue;
    result.push_back(V8DebuggerScript::Create(m_isolate, script, false));
  }
}

String16 V8Debugger::setBreakpoint(const ScriptBreakpoint& breakpoint,
                                   int* actualLineNumber,
                                   int* actualColumnNumber) {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::Context> context = debuggerContext();
  v8::Context::Scope contextScope(context);

  v8::Local<v8::Object> info = v8::Object::New(m_isolate);
  bool success = false;
  success = info->Set(context, toV8StringInternalized(m_isolate, "sourceID"),
                      toV8String(m_isolate, breakpoint.script_id))
                .FromMaybe(false);
  DCHECK(success);
  success = info->Set(context, toV8StringInternalized(m_isolate, "lineNumber"),
                      v8::Integer::New(m_isolate, breakpoint.line_number))
                .FromMaybe(false);
  DCHECK(success);
  success =
      info->Set(context, toV8StringInternalized(m_isolate, "columnNumber"),
                v8::Integer::New(m_isolate, breakpoint.column_number))
          .FromMaybe(false);
  DCHECK(success);
  success = info->Set(context, toV8StringInternalized(m_isolate, "condition"),
                      toV8String(m_isolate, breakpoint.condition))
                .FromMaybe(false);
  DCHECK(success);

  v8::Local<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(
      m_debuggerScript.Get(m_isolate)
          ->Get(context, toV8StringInternalized(m_isolate, "setBreakpoint"))
          .ToLocalChecked());
  v8::Local<v8::Value> breakpointId =
      v8::debug::Call(debuggerContext(), setBreakpointFunction, info)
          .ToLocalChecked();
  if (!breakpointId->IsString()) return "";
  *actualLineNumber =
      info->Get(context, toV8StringInternalized(m_isolate, "lineNumber"))
          .ToLocalChecked()
          ->Int32Value(context)
          .FromJust();
  *actualColumnNumber =
      info->Get(context, toV8StringInternalized(m_isolate, "columnNumber"))
          .ToLocalChecked()
          ->Int32Value(context)
          .FromJust();
  return toProtocolString(breakpointId.As<v8::String>());
}

void V8Debugger::removeBreakpoint(const String16& breakpointId) {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::Context> context = debuggerContext();
  v8::Context::Scope contextScope(context);

  v8::Local<v8::Object> info = v8::Object::New(m_isolate);
  bool success = false;
  success =
      info->Set(context, toV8StringInternalized(m_isolate, "breakpointId"),
                toV8String(m_isolate, breakpointId))
          .FromMaybe(false);
  DCHECK(success);

  v8::Local<v8::Function> removeBreakpointFunction =
      v8::Local<v8::Function>::Cast(
          m_debuggerScript.Get(m_isolate)
              ->Get(context,
                    toV8StringInternalized(m_isolate, "removeBreakpoint"))
              .ToLocalChecked());
  v8::debug::Call(debuggerContext(), removeBreakpointFunction, info)
      .ToLocalChecked();
}

void V8Debugger::clearBreakpoints() {
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::Context> context = debuggerContext();
  v8::Context::Scope contextScope(context);

  v8::Local<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(
      m_debuggerScript.Get(m_isolate)
          ->Get(context, toV8StringInternalized(m_isolate, "clearBreakpoints"))
          .ToLocalChecked());
  v8::debug::Call(debuggerContext(), clearBreakpoints).ToLocalChecked();
}

void V8Debugger::setBreakpointsActivated(bool activated) {
  if (!enabled()) {
    UNREACHABLE();
    return;
  }
  v8::HandleScope scope(m_isolate);
  v8::Local<v8::Context> context = debuggerContext();
  v8::Context::Scope contextScope(context);

  v8::Local<v8::Object> info = v8::Object::New(m_isolate);
  bool success = false;
  success = info->Set(context, toV8StringInternalized(m_isolate, "enabled"),
                      v8::Boolean::New(m_isolate, activated))
                .FromMaybe(false);
  DCHECK(success);
  v8::Local<v8::Function> setBreakpointsActivated =
      v8::Local<v8::Function>::Cast(
          m_debuggerScript.Get(m_isolate)
              ->Get(context, toV8StringInternalized(m_isolate,
                                                    "setBreakpointsActivated"))
              .ToLocalChecked());
  v8::debug::Call(debuggerContext(), setBreakpointsActivated, info)
      .ToLocalChecked();

  m_breakpointsActivated = activated;
}

v8::debug::ExceptionBreakState V8Debugger::getPauseOnExceptionsState() {
  DCHECK(enabled());
  return m_pauseOnExceptionsState;
}

void V8Debugger::setPauseOnExceptionsState(
    v8::debug::ExceptionBreakState pauseOnExceptionsState) {
  DCHECK(enabled());
  if (m_pauseOnExceptionsState == pauseOnExceptionsState) return;
  v8::debug::ChangeBreakOnException(m_isolate, pauseOnExceptionsState);
  m_pauseOnExceptionsState = pauseOnExceptionsState;
}

void V8Debugger::setPauseOnNextStatement(bool pause) {
  if (m_runningNestedMessageLoop) return;
  if (pause)
    v8::debug::DebugBreak(m_isolate);
  else
    v8::debug::CancelDebugBreak(m_isolate);
}

bool V8Debugger::canBreakProgram() {
  if (!m_breakpointsActivated) return false;
  return m_isolate->InContext();
}

void V8Debugger::breakProgram() {
  if (isPaused()) {
    DCHECK(!m_runningNestedMessageLoop);
    v8::Local<v8::Value> exception;
    v8::Local<v8::Array> hitBreakpoints;
    handleProgramBreak(m_pausedContext, m_executionState, exception,
                       hitBreakpoints);
    return;
  }

  if (!canBreakProgram()) return;

  v8::HandleScope scope(m_isolate);
  v8::Local<v8::Function> breakFunction;
  if (!v8::Function::New(m_isolate->GetCurrentContext(),
                         &V8Debugger::breakProgramCallback,
                         v8::External::New(m_isolate, this), 0,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&breakFunction))
    return;
  v8::debug::Call(debuggerContext(), breakFunction).ToLocalChecked();
}

void V8Debugger::continueProgram() {
  if (isPaused()) m_inspector->client()->quitMessageLoopOnPause();
  m_pausedContext.Clear();
  m_executionState.Clear();
}

void V8Debugger::stepIntoStatement() {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  v8::debug::PrepareStep(m_isolate, v8::debug::StepIn);
  continueProgram();
}

void V8Debugger::stepOverStatement() {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  v8::debug::PrepareStep(m_isolate, v8::debug::StepNext);
  continueProgram();
}

void V8Debugger::stepOutOfFunction() {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  v8::debug::PrepareStep(m_isolate, v8::debug::StepOut);
  continueProgram();
}

void V8Debugger::clearStepping() {
  DCHECK(enabled());
  v8::debug::ClearStepping(m_isolate);
}

Response V8Debugger::setScriptSource(
    const String16& sourceID, v8::Local<v8::String> newSource, bool dryRun,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails,
    JavaScriptCallFrames* newCallFrames, Maybe<bool>* stackChanged,
    bool* compileError) {
  class EnableLiveEditScope {
   public:
    explicit EnableLiveEditScope(v8::Isolate* isolate) : m_isolate(isolate) {
      v8::debug::SetLiveEditEnabled(m_isolate, true);
      inLiveEditScope = true;
    }
    ~EnableLiveEditScope() {
      v8::debug::SetLiveEditEnabled(m_isolate, false);
      inLiveEditScope = false;
    }

   private:
    v8::Isolate* m_isolate;
  };

  *compileError = false;
  DCHECK(enabled());
  v8::HandleScope scope(m_isolate);

  std::unique_ptr<v8::Context::Scope> contextScope;
  if (!isPaused())
    contextScope.reset(new v8::Context::Scope(debuggerContext()));

  v8::Local<v8::Value> argv[] = {toV8String(m_isolate, sourceID), newSource,
                                 v8Boolean(dryRun, m_isolate)};

  v8::Local<v8::Value> v8result;
  {
    EnableLiveEditScope enableLiveEditScope(m_isolate);
    v8::TryCatch tryCatch(m_isolate);
    tryCatch.SetVerbose(false);
    v8::MaybeLocal<v8::Value> maybeResult =
        callDebuggerMethod("liveEditScriptSource", 3, argv, false);
    if (tryCatch.HasCaught()) {
      v8::Local<v8::Message> message = tryCatch.Message();
      if (!message.IsEmpty())
        return Response::Error(toProtocolStringWithTypeCheck(message->Get()));
      else
        return Response::InternalError();
    }
    v8result = maybeResult.ToLocalChecked();
  }
  DCHECK(!v8result.IsEmpty());
  v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
  v8::Local<v8::Object> resultTuple =
      v8result->ToObject(context).ToLocalChecked();
  int code = static_cast<int>(resultTuple->Get(context, 0)
                                  .ToLocalChecked()
                                  ->ToInteger(context)
                                  .ToLocalChecked()
                                  ->Value());
  switch (code) {
    case 0: {
      *stackChanged = resultTuple->Get(context, 1)
                          .ToLocalChecked()
                          ->BooleanValue(context)
                          .FromJust();
      // Call stack may have changed after if the edited function was on the
      // stack.
      if (!dryRun && isPaused()) {
        JavaScriptCallFrames frames = currentCallFrames();
        newCallFrames->swap(frames);
      }
      return Response::OK();
    }
    // Compile error.
    case 1: {
      *exceptionDetails =
          protocol::Runtime::ExceptionDetails::create()
              .setExceptionId(m_inspector->nextExceptionId())
              .setText(toProtocolStringWithTypeCheck(
                  resultTuple->Get(context, 2).ToLocalChecked()))
              .setLineNumber(static_cast<int>(resultTuple->Get(context, 3)
                                                  .ToLocalChecked()
                                                  ->ToInteger(context)
                                                  .ToLocalChecked()
                                                  ->Value()) -
                             1)
              .setColumnNumber(static_cast<int>(resultTuple->Get(context, 4)
                                                    .ToLocalChecked()
                                                    ->ToInteger(context)
                                                    .ToLocalChecked()
                                                    ->Value()) -
                               1)
              .build();
      *compileError = true;
      return Response::OK();
    }
  }
  return Response::InternalError();
}

JavaScriptCallFrames V8Debugger::currentCallFrames(int limit) {
  if (!m_isolate->InContext()) return JavaScriptCallFrames();
  v8::Local<v8::Value> currentCallFramesV8;
  if (m_executionState.IsEmpty()) {
    v8::Local<v8::Function> currentCallFramesFunction =
        v8::Local<v8::Function>::Cast(
            m_debuggerScript.Get(m_isolate)
                ->Get(debuggerContext(),
                      toV8StringInternalized(m_isolate, "currentCallFrames"))
                .ToLocalChecked());
    if (!v8::debug::Call(debuggerContext(), currentCallFramesFunction,
                         v8::Integer::New(m_isolate, limit))
             .ToLocal(&currentCallFramesV8))
      return JavaScriptCallFrames();
  } else {
    v8::Local<v8::Value> argv[] = {m_executionState,
                                   v8::Integer::New(m_isolate, limit)};
    if (!callDebuggerMethod("currentCallFrames", arraysize(argv), argv, true)
             .ToLocal(&currentCallFramesV8))
      return JavaScriptCallFrames();
  }
  DCHECK(!currentCallFramesV8.IsEmpty());
  if (!currentCallFramesV8->IsArray()) return JavaScriptCallFrames();
  v8::Local<v8::Array> callFramesArray = currentCallFramesV8.As<v8::Array>();
  JavaScriptCallFrames callFrames;
  for (uint32_t i = 0; i < callFramesArray->Length(); ++i) {
    v8::Local<v8::Value> callFrameValue;
    if (!callFramesArray->Get(debuggerContext(), i).ToLocal(&callFrameValue))
      return JavaScriptCallFrames();
    if (!callFrameValue->IsObject()) return JavaScriptCallFrames();
    v8::Local<v8::Object> callFrameObject = callFrameValue.As<v8::Object>();
    callFrames.push_back(JavaScriptCallFrame::create(
        debuggerContext(), v8::Local<v8::Object>::Cast(callFrameObject)));
  }
  return callFrames;
}

static V8Debugger* toV8Debugger(v8::Local<v8::Value> data) {
  void* p = v8::Local<v8::External>::Cast(data)->Value();
  return static_cast<V8Debugger*>(p);
}

void V8Debugger::breakProgramCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK_EQ(info.Length(), 2);
  V8Debugger* thisPtr = toV8Debugger(info.Data());
  if (!thisPtr->enabled()) return;
  v8::Local<v8::Context> pausedContext =
      thisPtr->m_isolate->GetCurrentContext();
  v8::Local<v8::Value> exception;
  v8::Local<v8::Array> hitBreakpoints;
  thisPtr->handleProgramBreak(pausedContext,
                              v8::Local<v8::Object>::Cast(info[0]), exception,
                              hitBreakpoints);
}

void V8Debugger::handleProgramBreak(v8::Local<v8::Context> pausedContext,
                                    v8::Local<v8::Object> executionState,
                                    v8::Local<v8::Value> exception,
                                    v8::Local<v8::Array> hitBreakpointNumbers,
                                    bool isPromiseRejection, bool isUncaught) {
  // Don't allow nested breaks.
  if (m_runningNestedMessageLoop) return;

  V8DebuggerAgentImpl* agent = m_inspector->enabledDebuggerAgentForGroup(
      m_inspector->contextGroupId(pausedContext));
  if (!agent) return;

  std::vector<String16> breakpointIds;
  if (!hitBreakpointNumbers.IsEmpty()) {
    breakpointIds.reserve(hitBreakpointNumbers->Length());
    for (uint32_t i = 0; i < hitBreakpointNumbers->Length(); i++) {
      v8::Local<v8::Value> hitBreakpointNumber =
          hitBreakpointNumbers->Get(debuggerContext(), i).ToLocalChecked();
      DCHECK(hitBreakpointNumber->IsInt32());
      breakpointIds.push_back(String16::fromInteger(
          hitBreakpointNumber->Int32Value(debuggerContext()).FromJust()));
    }
  }

  m_pausedContext = pausedContext;
  m_executionState = executionState;
  V8DebuggerAgentImpl::SkipPauseRequest result = agent->didPause(
      pausedContext, exception, breakpointIds, isPromiseRejection, isUncaught);
  if (result == V8DebuggerAgentImpl::RequestNoSkip) {
    m_runningNestedMessageLoop = true;
    int groupId = m_inspector->contextGroupId(pausedContext);
    DCHECK(groupId);
    v8::Context::Scope scope(pausedContext);
    v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
    CHECK(!context.IsEmpty() &&
          context != v8::debug::GetDebugContext(m_isolate));
    m_inspector->client()->runMessageLoopOnPause(groupId);
    // The agent may have been removed in the nested loop.
    agent = m_inspector->enabledDebuggerAgentForGroup(
        m_inspector->contextGroupId(pausedContext));
    if (agent) agent->didContinue();
    m_runningNestedMessageLoop = false;
  }
  m_pausedContext.Clear();
  m_executionState.Clear();

  if (result == V8DebuggerAgentImpl::RequestStepFrame) {
    v8::debug::PrepareStep(m_isolate, v8::debug::StepFrame);
  } else if (result == V8DebuggerAgentImpl::RequestStepInto) {
    v8::debug::PrepareStep(m_isolate, v8::debug::StepIn);
  } else if (result == V8DebuggerAgentImpl::RequestStepOut) {
    v8::debug::PrepareStep(m_isolate, v8::debug::StepOut);
  }
}

void V8Debugger::v8DebugEventCallback(
    const v8::debug::EventDetails& eventDetails) {
  V8Debugger* thisPtr = toV8Debugger(eventDetails.GetCallbackData());
  thisPtr->handleV8DebugEvent(eventDetails);
}

v8::Local<v8::Value> V8Debugger::callInternalGetterFunction(
    v8::Local<v8::Object> object, const char* functionName) {
  v8::MicrotasksScope microtasks(m_isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> getterValue =
      object
          ->Get(m_isolate->GetCurrentContext(),
                toV8StringInternalized(m_isolate, functionName))
          .ToLocalChecked();
  DCHECK(!getterValue.IsEmpty() && getterValue->IsFunction());
  return v8::Local<v8::Function>::Cast(getterValue)
      ->Call(m_isolate->GetCurrentContext(), object, 0, nullptr)
      .ToLocalChecked();
}

void V8Debugger::handleV8DebugEvent(
    const v8::debug::EventDetails& eventDetails) {
  if (!enabled()) return;
  v8::HandleScope scope(m_isolate);

  v8::DebugEvent event = eventDetails.GetEvent();
  if (event != v8::Break && event != v8::Exception &&
      event != v8::AfterCompile && event != v8::CompileError)
    return;

  v8::Local<v8::Context> eventContext = eventDetails.GetEventContext();
  DCHECK(!eventContext.IsEmpty());
  V8DebuggerAgentImpl* agent = m_inspector->enabledDebuggerAgentForGroup(
      m_inspector->contextGroupId(eventContext));
  if (!agent) return;

  if (event == v8::AfterCompile || event == v8::CompileError) {
    v8::Context::Scope contextScope(debuggerContext());
    // Determine if the script is a wasm script.
    v8::Local<v8::Value> scriptMirror =
        callInternalGetterFunction(eventDetails.GetEventData(), "script");
    DCHECK(scriptMirror->IsObject());
    v8::Local<v8::Value> scriptWrapper =
        callInternalGetterFunction(scriptMirror.As<v8::Object>(), "value");
    DCHECK(scriptWrapper->IsObject());
    v8::Local<v8::debug::Script> script;
    if (!v8::debug::Script::Wrap(m_isolate, scriptWrapper.As<v8::Object>())
             .ToLocal(&script)) {
      return;
    }
    if (script->IsWasm()) {
      m_wasmTranslation.AddScript(script.As<v8::debug::WasmScript>(), agent);
    } else if (m_ignoreScriptParsedEventsCounter == 0) {
      agent->didParseSource(
          V8DebuggerScript::Create(m_isolate, script, inLiveEditScope),
          event == v8::AfterCompile);
    }
  } else if (event == v8::Exception) {
    v8::Local<v8::Context> context = debuggerContext();
    v8::Local<v8::Object> eventData = eventDetails.GetEventData();
    v8::Local<v8::Value> exception =
        callInternalGetterFunction(eventData, "exception");
    v8::Local<v8::Value> promise =
        callInternalGetterFunction(eventData, "promise");
    bool isPromiseRejection = !promise.IsEmpty() && promise->IsObject();
    v8::Local<v8::Value> uncaught =
        callInternalGetterFunction(eventData, "uncaught");
    bool isUncaught = uncaught->BooleanValue(context).FromJust();
    handleProgramBreak(eventContext, eventDetails.GetExecutionState(),
                       exception, v8::Local<v8::Array>(), isPromiseRejection,
                       isUncaught);
  } else if (event == v8::Break) {
    v8::Local<v8::Value> argv[] = {eventDetails.GetEventData()};
    v8::Local<v8::Value> hitBreakpoints;
    if (!callDebuggerMethod("getBreakpointNumbers", 1, argv, true)
             .ToLocal(&hitBreakpoints))
      return;
    DCHECK(hitBreakpoints->IsArray());
    handleProgramBreak(eventContext, eventDetails.GetExecutionState(),
                       v8::Local<v8::Value>(), hitBreakpoints.As<v8::Array>());
  }
}

void V8Debugger::v8AsyncTaskListener(v8::debug::PromiseDebugActionType type,
                                     int id, void* data) {
  V8Debugger* debugger = static_cast<V8Debugger*>(data);
  if (!debugger->m_maxAsyncCallStackDepth) return;
  // Async task events from Promises are given misaligned pointers to prevent
  // from overlapping with other Blink task identifiers. There is a single
  // namespace of such ids, managed by src/js/promise.js.
  void* ptr = reinterpret_cast<void*>(id * 2 + 1);
  switch (type) {
    case v8::debug::kDebugEnqueueAsyncFunction:
      debugger->asyncTaskScheduled("async function", ptr, true);
      break;
    case v8::debug::kDebugEnqueuePromiseResolve:
      debugger->asyncTaskScheduled("Promise.resolve", ptr, true);
      break;
    case v8::debug::kDebugEnqueuePromiseReject:
      debugger->asyncTaskScheduled("Promise.reject", ptr, true);
      break;
    case v8::debug::kDebugEnqueuePromiseResolveThenableJob:
      debugger->asyncTaskScheduled("PromiseResolveThenableJob", ptr, true);
      break;
    case v8::debug::kDebugPromiseCollected:
      debugger->asyncTaskCanceled(ptr);
      break;
    case v8::debug::kDebugWillHandle:
      debugger->asyncTaskStarted(ptr);
      break;
    case v8::debug::kDebugDidHandle:
      debugger->asyncTaskFinished(ptr);
      break;
  }
}

V8StackTraceImpl* V8Debugger::currentAsyncCallChain() {
  if (!m_currentStacks.size()) return nullptr;
  return m_currentStacks.back().get();
}

void V8Debugger::compileDebuggerScript() {
  if (!m_debuggerScript.IsEmpty()) {
    UNREACHABLE();
    return;
  }

  v8::HandleScope scope(m_isolate);
  v8::Context::Scope contextScope(debuggerContext());

  v8::Local<v8::String> scriptValue =
      v8::String::NewFromUtf8(m_isolate, DebuggerScript_js,
                              v8::NewStringType::kInternalized,
                              sizeof(DebuggerScript_js))
          .ToLocalChecked();
  v8::Local<v8::Value> value;
  if (!m_inspector->compileAndRunInternalScript(debuggerContext(), scriptValue)
           .ToLocal(&value)) {
    UNREACHABLE();
    return;
  }
  DCHECK(value->IsObject());
  m_debuggerScript.Reset(m_isolate, value.As<v8::Object>());
}

v8::Local<v8::Context> V8Debugger::debuggerContext() const {
  DCHECK(!m_debuggerContext.IsEmpty());
  return m_debuggerContext.Get(m_isolate);
}

v8::MaybeLocal<v8::Value> V8Debugger::getTargetScopes(
    v8::Local<v8::Context> context, v8::Local<v8::Value> value,
    ScopeTargetKind kind) {
  if (!enabled()) {
    UNREACHABLE();
    return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
  }
  v8::Local<v8::Value> argv[] = {value};
  v8::Local<v8::Value> scopesValue;

  const char* debuggerMethod = nullptr;
  switch (kind) {
    case FUNCTION:
      debuggerMethod = "getFunctionScopes";
      break;
    case GENERATOR:
      debuggerMethod = "getGeneratorScopes";
      break;
  }

  if (!callDebuggerMethod(debuggerMethod, 1, argv, true).ToLocal(&scopesValue))
    return v8::MaybeLocal<v8::Value>();
  v8::Local<v8::Value> copied;
  if (!copyValueFromDebuggerContext(m_isolate, debuggerContext(), context,
                                    scopesValue)
           .ToLocal(&copied) ||
      !copied->IsArray())
    return v8::MaybeLocal<v8::Value>();
  if (!markAsInternal(context, v8::Local<v8::Array>::Cast(copied),
                      V8InternalValueType::kScopeList))
    return v8::MaybeLocal<v8::Value>();
  if (!markArrayEntriesAsInternal(context, v8::Local<v8::Array>::Cast(copied),
                                  V8InternalValueType::kScope))
    return v8::MaybeLocal<v8::Value>();
  return copied;
}

v8::MaybeLocal<v8::Value> V8Debugger::functionScopes(
    v8::Local<v8::Context> context, v8::Local<v8::Function> function) {
  return getTargetScopes(context, function, FUNCTION);
}

v8::MaybeLocal<v8::Value> V8Debugger::generatorScopes(
    v8::Local<v8::Context> context, v8::Local<v8::Value> generator) {
  return getTargetScopes(context, generator, GENERATOR);
}

v8::MaybeLocal<v8::Array> V8Debugger::internalProperties(
    v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
  v8::Local<v8::Array> properties;
  if (!v8::debug::GetInternalProperties(m_isolate, value).ToLocal(&properties))
    return v8::MaybeLocal<v8::Array>();
  if (value->IsFunction()) {
    v8::Local<v8::Function> function = value.As<v8::Function>();
    v8::Local<v8::Value> location = functionLocation(context, function);
    if (location->IsObject()) {
      createDataProperty(
          context, properties, properties->Length(),
          toV8StringInternalized(m_isolate, "[[FunctionLocation]]"));
      createDataProperty(context, properties, properties->Length(), location);
    }
    if (function->IsGeneratorFunction()) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[IsGenerator]]"));
      createDataProperty(context, properties, properties->Length(),
                         v8::True(m_isolate));
    }
  }
  if (!enabled()) return properties;
  if (value->IsMap() || value->IsWeakMap() || value->IsSet() ||
      value->IsWeakSet() || value->IsSetIterator() || value->IsMapIterator()) {
    v8::Local<v8::Value> entries =
        collectionEntries(context, v8::Local<v8::Object>::Cast(value));
    if (entries->IsArray()) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[Entries]]"));
      createDataProperty(context, properties, properties->Length(), entries);
    }
  }
  if (value->IsGeneratorObject()) {
    v8::Local<v8::Value> location =
        generatorObjectLocation(context, v8::Local<v8::Object>::Cast(value));
    if (location->IsObject()) {
      createDataProperty(
          context, properties, properties->Length(),
          toV8StringInternalized(m_isolate, "[[GeneratorLocation]]"));
      createDataProperty(context, properties, properties->Length(), location);
    }
    v8::Local<v8::Value> scopes;
    if (generatorScopes(context, value).ToLocal(&scopes)) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[Scopes]]"));
      createDataProperty(context, properties, properties->Length(), scopes);
    }
  }
  if (value->IsFunction()) {
    v8::Local<v8::Function> function = value.As<v8::Function>();
    v8::Local<v8::Value> boundFunction = function->GetBoundFunction();
    v8::Local<v8::Value> scopes;
    if (boundFunction->IsUndefined() &&
        functionScopes(context, function).ToLocal(&scopes)) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[Scopes]]"));
      createDataProperty(context, properties, properties->Length(), scopes);
    }
  }
  return properties;
}

v8::Local<v8::Value> V8Debugger::collectionEntries(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object) {
  if (!enabled()) {
    UNREACHABLE();
    return v8::Undefined(m_isolate);
  }
  v8::Local<v8::Value> argv[] = {object};
  v8::Local<v8::Value> entriesValue;
  if (!callDebuggerMethod("getCollectionEntries", 1, argv, true)
           .ToLocal(&entriesValue) ||
      !entriesValue->IsArray())
    return v8::Undefined(m_isolate);

  v8::Local<v8::Array> entries = entriesValue.As<v8::Array>();
  v8::Local<v8::Array> copiedArray =
      v8::Array::New(m_isolate, entries->Length());
  if (!copiedArray->SetPrototype(context, v8::Null(m_isolate)).FromMaybe(false))
    return v8::Undefined(m_isolate);
  for (uint32_t i = 0; i < entries->Length(); ++i) {
    v8::Local<v8::Value> item;
    if (!entries->Get(debuggerContext(), i).ToLocal(&item))
      return v8::Undefined(m_isolate);
    v8::Local<v8::Value> copied;
    if (!copyValueFromDebuggerContext(m_isolate, debuggerContext(), context,
                                      item)
             .ToLocal(&copied))
      return v8::Undefined(m_isolate);
    if (!createDataProperty(context, copiedArray, i, copied).FromMaybe(false))
      return v8::Undefined(m_isolate);
  }
  if (!markArrayEntriesAsInternal(context,
                                  v8::Local<v8::Array>::Cast(copiedArray),
                                  V8InternalValueType::kEntry))
    return v8::Undefined(m_isolate);
  return copiedArray;
}

v8::Local<v8::Value> V8Debugger::generatorObjectLocation(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object) {
  if (!enabled()) {
    UNREACHABLE();
    return v8::Null(m_isolate);
  }
  v8::Local<v8::Value> argv[] = {object};
  v8::Local<v8::Value> location;
  v8::Local<v8::Value> copied;
  if (!callDebuggerMethod("getGeneratorObjectLocation", 1, argv, true)
           .ToLocal(&location) ||
      !copyValueFromDebuggerContext(m_isolate, debuggerContext(), context,
                                    location)
           .ToLocal(&copied) ||
      !copied->IsObject())
    return v8::Null(m_isolate);
  if (!markAsInternal(context, v8::Local<v8::Object>::Cast(copied),
                      V8InternalValueType::kLocation))
    return v8::Null(m_isolate);
  return copied;
}

v8::Local<v8::Value> V8Debugger::functionLocation(
    v8::Local<v8::Context> context, v8::Local<v8::Function> function) {
  int scriptId = function->ScriptId();
  if (scriptId == v8::UnboundScript::kNoScriptId) return v8::Null(m_isolate);
  int lineNumber = function->GetScriptLineNumber();
  int columnNumber = function->GetScriptColumnNumber();
  if (lineNumber == v8::Function::kLineOffsetNotFound ||
      columnNumber == v8::Function::kLineOffsetNotFound)
    return v8::Null(m_isolate);
  v8::Local<v8::Object> location = v8::Object::New(m_isolate);
  if (!location->SetPrototype(context, v8::Null(m_isolate)).FromMaybe(false))
    return v8::Null(m_isolate);
  if (!createDataProperty(
           context, location, toV8StringInternalized(m_isolate, "scriptId"),
           toV8String(m_isolate, String16::fromInteger(scriptId)))
           .FromMaybe(false))
    return v8::Null(m_isolate);
  if (!createDataProperty(context, location,
                          toV8StringInternalized(m_isolate, "lineNumber"),
                          v8::Integer::New(m_isolate, lineNumber))
           .FromMaybe(false))
    return v8::Null(m_isolate);
  if (!createDataProperty(context, location,
                          toV8StringInternalized(m_isolate, "columnNumber"),
                          v8::Integer::New(m_isolate, columnNumber))
           .FromMaybe(false))
    return v8::Null(m_isolate);
  if (!markAsInternal(context, location, V8InternalValueType::kLocation))
    return v8::Null(m_isolate);
  return location;
}

bool V8Debugger::isPaused() { return !m_pausedContext.IsEmpty(); }

std::unique_ptr<V8StackTraceImpl> V8Debugger::createStackTrace(
    v8::Local<v8::StackTrace> stackTrace) {
  int contextGroupId =
      m_isolate->InContext()
          ? m_inspector->contextGroupId(m_isolate->GetCurrentContext())
          : 0;
  return V8StackTraceImpl::create(this, contextGroupId, stackTrace,
                                  V8StackTraceImpl::maxCallStackSizeToCapture);
}

void V8Debugger::setAsyncCallStackDepth(V8DebuggerAgentImpl* agent, int depth) {
  if (depth <= 0)
    m_maxAsyncCallStackDepthMap.erase(agent);
  else
    m_maxAsyncCallStackDepthMap[agent] = depth;

  int maxAsyncCallStackDepth = 0;
  for (const auto& pair : m_maxAsyncCallStackDepthMap) {
    if (pair.second > maxAsyncCallStackDepth)
      maxAsyncCallStackDepth = pair.second;
  }

  if (m_maxAsyncCallStackDepth == maxAsyncCallStackDepth) return;
  m_maxAsyncCallStackDepth = maxAsyncCallStackDepth;
  if (!maxAsyncCallStackDepth) allAsyncTasksCanceled();
}

void V8Debugger::asyncTaskScheduled(const StringView& taskName, void* task,
                                    bool recurring) {
  if (!m_maxAsyncCallStackDepth) return;
  asyncTaskScheduled(toString16(taskName), task, recurring);
}

void V8Debugger::asyncTaskScheduled(const String16& taskName, void* task,
                                    bool recurring) {
  if (!m_maxAsyncCallStackDepth) return;
  v8::HandleScope scope(m_isolate);
  int contextGroupId =
      m_isolate->InContext()
          ? m_inspector->contextGroupId(m_isolate->GetCurrentContext())
          : 0;
  std::unique_ptr<V8StackTraceImpl> chain = V8StackTraceImpl::capture(
      this, contextGroupId, V8StackTraceImpl::maxCallStackSizeToCapture,
      taskName);
  if (chain) {
    m_asyncTaskStacks[task] = std::move(chain);
    if (recurring) m_recurringTasks.insert(task);
    int id = ++m_lastTaskId;
    m_taskToId[task] = id;
    m_idToTask[id] = task;
    if (static_cast<int>(m_idToTask.size()) > m_maxAsyncCallStacks) {
      void* taskToRemove = m_idToTask.begin()->second;
      asyncTaskCanceled(taskToRemove);
    }
  }
}

void V8Debugger::asyncTaskCanceled(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
  m_asyncTaskStacks.erase(task);
  m_recurringTasks.erase(task);
  auto it = m_taskToId.find(task);
  if (it == m_taskToId.end()) return;
  m_idToTask.erase(it->second);
  m_taskToId.erase(it);
}

void V8Debugger::asyncTaskStarted(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
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

void V8Debugger::asyncTaskFinished(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
  // We could start instrumenting half way and the stack is empty.
  if (!m_currentStacks.size()) return;

  DCHECK(m_currentTasks.back() == task);
  m_currentTasks.pop_back();

  m_currentStacks.pop_back();
  if (m_recurringTasks.find(task) == m_recurringTasks.end()) {
    m_asyncTaskStacks.erase(task);
    auto it = m_taskToId.find(task);
    if (it == m_taskToId.end()) return;
    m_idToTask.erase(it->second);
    m_taskToId.erase(it);
  }
}

void V8Debugger::allAsyncTasksCanceled() {
  m_asyncTaskStacks.clear();
  m_recurringTasks.clear();
  m_currentStacks.clear();
  m_currentTasks.clear();
  m_idToTask.clear();
  m_taskToId.clear();
  m_lastTaskId = 0;
}

void V8Debugger::muteScriptParsedEvents() {
  ++m_ignoreScriptParsedEventsCounter;
}

void V8Debugger::unmuteScriptParsedEvents() {
  --m_ignoreScriptParsedEventsCounter;
  DCHECK_GE(m_ignoreScriptParsedEventsCounter, 0);
}

std::unique_ptr<V8StackTraceImpl> V8Debugger::captureStackTrace(
    bool fullStack) {
  if (!m_isolate->InContext()) return nullptr;

  v8::HandleScope handles(m_isolate);
  int contextGroupId =
      m_inspector->contextGroupId(m_isolate->GetCurrentContext());
  if (!contextGroupId) return nullptr;

  size_t stackSize =
      fullStack ? V8StackTraceImpl::maxCallStackSizeToCapture : 1;
  if (m_inspector->enabledRuntimeAgentForGroup(contextGroupId))
    stackSize = V8StackTraceImpl::maxCallStackSizeToCapture;

  return V8StackTraceImpl::capture(this, contextGroupId, stackSize);
}

}  // namespace v8_inspector
