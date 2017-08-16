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

static const int kMaxAsyncTaskStacks = 128 * 1024;

inline v8::Local<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate) {
  return value ? v8::True(isolate) : v8::False(isolate);
}

V8DebuggerAgentImpl* agentForScript(V8InspectorImpl* inspector,
                                    v8::Local<v8::debug::Script> script) {
  int contextId;
  if (!script->ContextId().To(&contextId)) return nullptr;
  int contextGroupId = inspector->contextGroupId(contextId);
  if (!contextGroupId) return nullptr;
  return inspector->enabledDebuggerAgentForGroup(contextGroupId);
}

v8::MaybeLocal<v8::Array> collectionsEntries(v8::Local<v8::Context> context,
                                             v8::Local<v8::Value> value) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Array> entries;
  bool isKeyValue = false;
  if (!v8::debug::EntriesPreview(isolate, value, &isKeyValue).ToLocal(&entries))
    return v8::MaybeLocal<v8::Array>();

  v8::Local<v8::Array> wrappedEntries = v8::Array::New(isolate);
  CHECK(!isKeyValue || wrappedEntries->Length() % 2 == 0);
  if (!wrappedEntries->SetPrototype(context, v8::Null(isolate))
           .FromMaybe(false))
    return v8::MaybeLocal<v8::Array>();
  for (uint32_t i = 0; i < entries->Length(); i += isKeyValue ? 2 : 1) {
    v8::Local<v8::Value> item;
    if (!entries->Get(context, i).ToLocal(&item)) continue;
    v8::Local<v8::Value> value;
    if (isKeyValue && !entries->Get(context, i + 1).ToLocal(&value)) continue;
    v8::Local<v8::Object> wrapper = v8::Object::New(isolate);
    if (!wrapper->SetPrototype(context, v8::Null(isolate)).FromMaybe(false))
      continue;
    createDataProperty(
        context, wrapper,
        toV8StringInternalized(isolate, isKeyValue ? "key" : "value"), item);
    if (isKeyValue) {
      createDataProperty(context, wrapper,
                         toV8StringInternalized(isolate, "value"), value);
    }
    createDataProperty(context, wrappedEntries, wrappedEntries->Length(),
                       wrapper);
  }
  if (!markArrayEntriesAsInternal(context, wrappedEntries,
                                  V8InternalValueType::kEntry)) {
    return v8::MaybeLocal<v8::Array>();
  }
  return wrappedEntries;
}

v8::MaybeLocal<v8::Object> buildLocation(v8::Local<v8::Context> context,
                                         int scriptId, int lineNumber,
                                         int columnNumber) {
  if (scriptId == v8::UnboundScript::kNoScriptId)
    return v8::MaybeLocal<v8::Object>();
  if (lineNumber == v8::Function::kLineOffsetNotFound ||
      columnNumber == v8::Function::kLineOffsetNotFound) {
    return v8::MaybeLocal<v8::Object>();
  }
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> location = v8::Object::New(isolate);
  if (!location->SetPrototype(context, v8::Null(isolate)).FromMaybe(false)) {
    return v8::MaybeLocal<v8::Object>();
  }
  if (!createDataProperty(context, location,
                          toV8StringInternalized(isolate, "scriptId"),
                          toV8String(isolate, String16::fromInteger(scriptId)))
           .FromMaybe(false)) {
    return v8::MaybeLocal<v8::Object>();
  }
  if (!createDataProperty(context, location,
                          toV8StringInternalized(isolate, "lineNumber"),
                          v8::Integer::New(isolate, lineNumber))
           .FromMaybe(false)) {
    return v8::MaybeLocal<v8::Object>();
  }
  if (!createDataProperty(context, location,
                          toV8StringInternalized(isolate, "columnNumber"),
                          v8::Integer::New(isolate, columnNumber))
           .FromMaybe(false)) {
    return v8::MaybeLocal<v8::Object>();
  }
  if (!markAsInternal(context, location, V8InternalValueType::kLocation)) {
    return v8::MaybeLocal<v8::Object>();
  }
  return location;
}

v8::MaybeLocal<v8::Object> generatorObjectLocation(
    v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
  if (!value->IsGeneratorObject()) return v8::MaybeLocal<v8::Object>();
  v8::Local<v8::debug::GeneratorObject> generatorObject =
      v8::debug::GeneratorObject::Cast(value);
  if (!generatorObject->IsSuspended()) {
    v8::Local<v8::Function> func = generatorObject->Function();
    return buildLocation(context, func->ScriptId(), func->GetScriptLineNumber(),
                         func->GetScriptColumnNumber());
  }
  v8::Local<v8::debug::Script> script;
  if (!generatorObject->Script().ToLocal(&script))
    return v8::MaybeLocal<v8::Object>();
  v8::debug::Location suspendedLocation = generatorObject->SuspendedLocation();
  return buildLocation(context, script->Id(), suspendedLocation.GetLineNumber(),
                       suspendedLocation.GetColumnNumber());
}

template <typename Map>
void cleanupExpiredWeakPointers(Map& map) {
  for (auto it = map.begin(); it != map.end();) {
    if (it->second.expired()) {
      it = map.erase(it);
    } else {
      ++it;
    }
  }
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
      m_ignoreScriptParsedEventsCounter(0),
      m_maxAsyncCallStacks(kMaxAsyncTaskStacks),
      m_maxAsyncCallStackDepth(0),
      m_pauseOnExceptionsState(v8::debug::NoBreakOnException),
      m_wasmTranslation(isolate) {}

V8Debugger::~V8Debugger() {}

void V8Debugger::enable() {
  if (m_enableCount++) return;
  DCHECK(!enabled());
  v8::HandleScope scope(m_isolate);
  v8::debug::SetDebugDelegate(m_isolate, this);
  v8::debug::SetOutOfMemoryCallback(m_isolate, &V8Debugger::v8OOMCallback,
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
  clearContinueToLocation();
  m_debuggerScript.Reset();
  m_debuggerContext.Reset();
  allAsyncTasksCanceled();
  m_taskWithScheduledBreak = nullptr;
  m_wasmTranslation.Clear();
  v8::debug::SetDebugDelegate(m_isolate, nullptr);
  v8::debug::SetOutOfMemoryCallback(m_isolate, nullptr, nullptr);
  m_isolate->RestoreOriginalHeapLimit();
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
    if (script->IsEmbedded()) {
      result.push_back(V8DebuggerScript::Create(m_isolate, script, false));
      continue;
    }
    int contextId;
    if (!script->ContextId().To(&contextId)) continue;
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
  USE(success);

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
  USE(success);

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
  v8::debug::SetBreakPointsActive(m_isolate, activated);
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

void V8Debugger::setPauseOnNextStatement(bool pause, int targetContextGroupId) {
  if (isPaused()) return;
  DCHECK(targetContextGroupId);
  if (!pause && m_targetContextGroupId &&
      m_targetContextGroupId != targetContextGroupId) {
    return;
  }
  m_targetContextGroupId = targetContextGroupId;
  m_breakRequested = pause;
  if (pause)
    v8::debug::DebugBreak(m_isolate);
  else
    v8::debug::CancelDebugBreak(m_isolate);
}

bool V8Debugger::canBreakProgram() {
  if (!m_breakpointsActivated) return false;
  return !v8::debug::AllFramesOnStackAreBlackboxed(m_isolate);
}

bool V8Debugger::breakProgram(int targetContextGroupId) {
  // Don't allow nested breaks.
  if (isPaused()) return true;
  if (!canBreakProgram()) return true;
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::BreakRightNow(m_isolate);
  return m_inspector->enabledDebuggerAgentForGroup(targetContextGroupId);
}

void V8Debugger::continueProgram(int targetContextGroupId) {
  if (m_pausedContextGroupId != targetContextGroupId) return;
  if (isPaused()) m_inspector->client()->quitMessageLoopOnPause();
  m_pausedContext.Clear();
  m_executionState.Clear();
}

void V8Debugger::stepIntoStatement(int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::PrepareStep(m_isolate, v8::debug::StepIn);
  continueProgram(targetContextGroupId);
}

void V8Debugger::stepOverStatement(int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::PrepareStep(m_isolate, v8::debug::StepNext);
  continueProgram(targetContextGroupId);
}

void V8Debugger::stepOutOfFunction(int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::PrepareStep(m_isolate, v8::debug::StepOut);
  continueProgram(targetContextGroupId);
}

void V8Debugger::scheduleStepIntoAsync(
    std::unique_ptr<ScheduleStepIntoAsyncCallback> callback,
    int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  DCHECK(targetContextGroupId);
  if (m_stepIntoAsyncCallback) {
    m_stepIntoAsyncCallback->sendFailure(Response::Error(
        "Current scheduled step into async was overriden with new one."));
  }
  m_targetContextGroupId = targetContextGroupId;
  m_stepIntoAsyncCallback = std::move(callback);
}

Response V8Debugger::continueToLocation(
    int targetContextGroupId,
    std::unique_ptr<protocol::Debugger::Location> location,
    const String16& targetCallFrames) {
  DCHECK(isPaused());
  DCHECK(!m_executionState.IsEmpty());
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  ScriptBreakpoint breakpoint(location->getScriptId(),
                              location->getLineNumber(),
                              location->getColumnNumber(0), String16());
  int lineNumber = 0;
  int columnNumber = 0;
  m_continueToLocationBreakpointId =
      setBreakpoint(breakpoint, &lineNumber, &columnNumber);
  if (!m_continueToLocationBreakpointId.isEmpty()) {
    m_continueToLocationTargetCallFrames = targetCallFrames;
    if (m_continueToLocationTargetCallFrames !=
        protocol::Debugger::ContinueToLocation::TargetCallFramesEnum::Any) {
      m_continueToLocationStack = captureStackTrace(true);
      DCHECK(m_continueToLocationStack);
    }
    continueProgram(targetContextGroupId);
    // TODO(kozyatinskiy): Return actual line and column number.
    return Response::OK();
  } else {
    return Response::Error("Cannot continue to specified location");
  }
}

bool V8Debugger::shouldContinueToCurrentLocation() {
  if (m_continueToLocationTargetCallFrames ==
      protocol::Debugger::ContinueToLocation::TargetCallFramesEnum::Any) {
    return true;
  }
  std::unique_ptr<V8StackTraceImpl> currentStack = captureStackTrace(true);
  if (m_continueToLocationTargetCallFrames ==
      protocol::Debugger::ContinueToLocation::TargetCallFramesEnum::Current) {
    return m_continueToLocationStack->isEqualIgnoringTopFrame(
        currentStack.get());
  }
  return true;
}

void V8Debugger::clearContinueToLocation() {
  if (m_continueToLocationBreakpointId.isEmpty()) return;
  removeBreakpoint(m_continueToLocationBreakpointId);
  m_continueToLocationBreakpointId = String16();
  m_continueToLocationTargetCallFrames = String16();
  m_continueToLocationStack.reset();
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
  if (!isPaused()) return JavaScriptCallFrames();
  v8::Local<v8::Value> currentCallFramesV8;
  v8::Local<v8::Value> argv[] = {m_executionState,
                                 v8::Integer::New(m_isolate, limit)};
  if (!callDebuggerMethod("currentCallFrames", arraysize(argv), argv, true)
           .ToLocal(&currentCallFramesV8)) {
    return JavaScriptCallFrames();
  }
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

void V8Debugger::handleProgramBreak(v8::Local<v8::Context> pausedContext,
                                    v8::Local<v8::Object> executionState,
                                    v8::Local<v8::Value> exception,
                                    v8::Local<v8::Array> hitBreakpointNumbers,
                                    bool isPromiseRejection, bool isUncaught) {
  // Don't allow nested breaks.
  if (isPaused()) return;

  int contextGroupId = m_inspector->contextGroupId(pausedContext);
  if (m_targetContextGroupId && contextGroupId != m_targetContextGroupId) {
    v8::debug::PrepareStep(m_isolate, v8::debug::StepOut);
    return;
  }
  m_targetContextGroupId = 0;
  if (m_stepIntoAsyncCallback) {
    m_stepIntoAsyncCallback->sendFailure(
        Response::Error("No async tasks were scheduled before pause."));
    m_stepIntoAsyncCallback.reset();
  }
  m_breakRequested = false;
  V8DebuggerAgentImpl* agent = m_inspector->enabledDebuggerAgentForGroup(
      m_inspector->contextGroupId(pausedContext));
  if (!agent || (agent->skipAllPauses() && !m_scheduledOOMBreak)) return;

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
    if (breakpointIds.size() == 1 &&
        breakpointIds[0] == m_continueToLocationBreakpointId) {
      v8::Context::Scope contextScope(pausedContext);
      if (!shouldContinueToCurrentLocation()) return;
    }
  }
  clearContinueToLocation();

  m_pausedContext = pausedContext;
  m_executionState = executionState;
  m_pausedContextGroupId = contextGroupId;
  agent->didPause(InspectedContext::contextId(pausedContext), exception,
                  breakpointIds, isPromiseRejection, isUncaught,
                  m_scheduledOOMBreak);
  int groupId = m_inspector->contextGroupId(pausedContext);
  DCHECK(groupId);
  {
    v8::Context::Scope scope(pausedContext);
    v8::Local<v8::Context> context = m_isolate->GetCurrentContext();
    CHECK(!context.IsEmpty() &&
          context != v8::debug::GetDebugContext(m_isolate));
    m_inspector->client()->runMessageLoopOnPause(groupId);
    m_pausedContextGroupId = 0;
  }
  // The agent may have been removed in the nested loop.
  agent = m_inspector->enabledDebuggerAgentForGroup(groupId);
  if (agent) agent->didContinue();
  if (m_scheduledOOMBreak) m_isolate->RestoreOriginalHeapLimit();
  m_scheduledOOMBreak = false;
  m_pausedContext.Clear();
  m_executionState.Clear();
}

void V8Debugger::v8OOMCallback(void* data) {
  V8Debugger* thisPtr = static_cast<V8Debugger*>(data);
  thisPtr->m_isolate->IncreaseHeapLimitForDebugging();
  thisPtr->m_scheduledOOMBreak = true;
  v8::Local<v8::Context> context = thisPtr->m_isolate->GetEnteredContext();
  DCHECK(!context.IsEmpty());
  thisPtr->setPauseOnNextStatement(
      true, thisPtr->m_inspector->contextGroupId(context));
}

void V8Debugger::ScriptCompiled(v8::Local<v8::debug::Script> script,
                                bool has_compile_error) {
  V8DebuggerAgentImpl* agent = agentForScript(m_inspector, script);
  if (!agent) return;
  if (script->IsWasm()) {
    m_wasmTranslation.AddScript(script.As<v8::debug::WasmScript>(), agent);
  } else if (m_ignoreScriptParsedEventsCounter == 0) {
    agent->didParseSource(
        V8DebuggerScript::Create(m_isolate, script, inLiveEditScope),
        !has_compile_error);
  }
}

void V8Debugger::BreakProgramRequested(v8::Local<v8::Context> pausedContext,
                                       v8::Local<v8::Object> execState,
                                       v8::Local<v8::Value> breakPointsHit) {
  v8::Local<v8::Value> argv[] = {breakPointsHit};
  v8::Local<v8::Value> hitBreakpoints;
  if (!callDebuggerMethod("getBreakpointNumbers", 1, argv, true)
           .ToLocal(&hitBreakpoints)) {
    return;
  }
  DCHECK(hitBreakpoints->IsArray());
  handleProgramBreak(pausedContext, execState, v8::Local<v8::Value>(),
                     hitBreakpoints.As<v8::Array>());
}

void V8Debugger::ExceptionThrown(v8::Local<v8::Context> pausedContext,
                                 v8::Local<v8::Object> execState,
                                 v8::Local<v8::Value> exception,
                                 v8::Local<v8::Value> promise,
                                 bool isUncaught) {
  bool isPromiseRejection = promise->IsPromise();
  handleProgramBreak(pausedContext, execState, exception,
                     v8::Local<v8::Array>(), isPromiseRejection, isUncaught);
}

bool V8Debugger::IsFunctionBlackboxed(v8::Local<v8::debug::Script> script,
                                      const v8::debug::Location& start,
                                      const v8::debug::Location& end) {
  V8DebuggerAgentImpl* agent = agentForScript(m_inspector, script);
  if (!agent) return false;
  return agent->isFunctionBlackboxed(String16::fromInteger(script->Id()), start,
                                     end);
}

void V8Debugger::PromiseEventOccurred(v8::debug::PromiseDebugActionType type,
                                      int id, int parentId,
                                      bool createdByUser) {
  // Async task events from Promises are given misaligned pointers to prevent
  // from overlapping with other Blink task identifiers.
  void* task = reinterpret_cast<void*>(id * 2 + 1);
  void* parentTask =
      parentId ? reinterpret_cast<void*>(parentId * 2 + 1) : nullptr;
  switch (type) {
    case v8::debug::kDebugPromiseCreated:
      asyncTaskCreatedForStack(task, parentTask);
      if (createdByUser && parentTask) asyncTaskCandidateForStepping(task);
      break;
    case v8::debug::kDebugEnqueueAsyncFunction:
      asyncTaskScheduledForStack("async function", task, true);
      break;
    case v8::debug::kDebugEnqueuePromiseResolve:
      asyncTaskScheduledForStack("Promise.resolve", task, true);
      break;
    case v8::debug::kDebugEnqueuePromiseReject:
      asyncTaskScheduledForStack("Promise.reject", task, true);
      break;
    case v8::debug::kDebugWillHandle:
      asyncTaskStartedForStack(task);
      asyncTaskStartedForStepping(task);
      break;
    case v8::debug::kDebugDidHandle:
      asyncTaskFinishedForStack(task);
      asyncTaskFinishedForStepping(task);
      break;
  }
}

std::shared_ptr<AsyncStackTrace> V8Debugger::currentAsyncParent() {
  // TODO(kozyatinskiy): implement creation chain as parent without hack.
  if (!m_currentAsyncCreation.empty() && m_currentAsyncCreation.back()) {
    return m_currentAsyncCreation.back();
  }
  return m_currentAsyncParent.empty() ? nullptr : m_currentAsyncParent.back();
}

std::shared_ptr<AsyncStackTrace> V8Debugger::currentAsyncCreation() {
  return nullptr;
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
    v8::Local<v8::Object> location;
    if (buildLocation(context, function->ScriptId(),
                      function->GetScriptLineNumber(),
                      function->GetScriptColumnNumber())
            .ToLocal(&location)) {
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
  v8::Local<v8::Array> entries;
  if (collectionsEntries(context, value).ToLocal(&entries)) {
    createDataProperty(context, properties, properties->Length(),
                       toV8StringInternalized(m_isolate, "[[Entries]]"));
    createDataProperty(context, properties, properties->Length(), entries);
  }
  if (value->IsGeneratorObject()) {
    v8::Local<v8::Object> location;
    if (generatorObjectLocation(context, value).ToLocal(&location)) {
      createDataProperty(
          context, properties, properties->Length(),
          toV8StringInternalized(m_isolate, "[[GeneratorLocation]]"));
      createDataProperty(context, properties, properties->Length(), location);
    }
    if (!enabled()) return properties;
    v8::Local<v8::Value> scopes;
    if (generatorScopes(context, value).ToLocal(&scopes)) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[Scopes]]"));
      createDataProperty(context, properties, properties->Length(), scopes);
    }
  }
  if (!enabled()) return properties;
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

std::unique_ptr<V8StackTraceImpl> V8Debugger::createStackTrace(
    v8::Local<v8::StackTrace> v8StackTrace) {
  return V8StackTraceImpl::create(this, currentContextGroupId(), v8StackTrace,
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

void V8Debugger::asyncTaskCreatedForStack(void* task, void* parentTask) {
  if (!m_maxAsyncCallStackDepth) return;
  if (parentTask) m_parentTask[task] = parentTask;
  v8::HandleScope scope(m_isolate);
  std::shared_ptr<AsyncStackTrace> asyncCreation =
      AsyncStackTrace::capture(this, currentContextGroupId(), String16(),
                               V8StackTraceImpl::maxCallStackSizeToCapture);
  // Passing one as maxStackSize forces no async chain for the new stack.
  if (asyncCreation && !asyncCreation->isEmpty()) {
    m_asyncTaskCreationStacks[task] = asyncCreation;
    m_allAsyncStacks.push_back(std::move(asyncCreation));
    ++m_asyncStacksCount;
    collectOldAsyncStacksIfNeeded();
  }
}

void V8Debugger::asyncTaskScheduled(const StringView& taskName, void* task,
                                    bool recurring) {
  asyncTaskScheduledForStack(toString16(taskName), task, recurring);
  asyncTaskCandidateForStepping(task);
}

void V8Debugger::asyncTaskCanceled(void* task) {
  asyncTaskCanceledForStack(task);
  asyncTaskCanceledForStepping(task);
}

void V8Debugger::asyncTaskStarted(void* task) {
  asyncTaskStartedForStack(task);
  asyncTaskStartedForStepping(task);
}

void V8Debugger::asyncTaskFinished(void* task) {
  asyncTaskFinishedForStack(task);
  asyncTaskFinishedForStepping(task);
}

void V8Debugger::asyncTaskScheduledForStack(const String16& taskName,
                                            void* task, bool recurring) {
  if (!m_maxAsyncCallStackDepth) return;
  v8::HandleScope scope(m_isolate);
  std::shared_ptr<AsyncStackTrace> asyncStack =
      AsyncStackTrace::capture(this, currentContextGroupId(), taskName,
                               V8StackTraceImpl::maxCallStackSizeToCapture);
  if (asyncStack) {
    m_asyncTaskStacks[task] = asyncStack;
    if (recurring) m_recurringTasks.insert(task);
    m_allAsyncStacks.push_back(std::move(asyncStack));
    ++m_asyncStacksCount;
    collectOldAsyncStacksIfNeeded();
  }
}

void V8Debugger::asyncTaskCanceledForStack(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
  m_asyncTaskStacks.erase(task);
  m_recurringTasks.erase(task);
  m_parentTask.erase(task);
  m_asyncTaskCreationStacks.erase(task);
}

void V8Debugger::asyncTaskStartedForStack(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
  // Needs to support following order of events:
  // - asyncTaskScheduled
  //   <-- attached here -->
  // - asyncTaskStarted
  // - asyncTaskCanceled <-- canceled before finished
  //   <-- async stack requested here -->
  // - asyncTaskFinished
  m_currentTasks.push_back(task);
  auto parentIt = m_parentTask.find(task);
  AsyncTaskToStackTrace::iterator stackIt = m_asyncTaskStacks.find(
      parentIt == m_parentTask.end() ? task : parentIt->second);
  if (stackIt != m_asyncTaskStacks.end()) {
    m_currentAsyncParent.push_back(stackIt->second.lock());
  } else {
    m_currentAsyncParent.emplace_back();
  }
  auto itCreation = m_asyncTaskCreationStacks.find(task);
  if (itCreation != m_asyncTaskCreationStacks.end()) {
    m_currentAsyncCreation.push_back(itCreation->second.lock());
    // TODO(kozyatinskiy): implement it without hack.
    if (m_currentAsyncParent.back()) {
      m_currentAsyncCreation.back()->setDescription(
          m_currentAsyncParent.back()->description());
      m_currentAsyncParent.back().reset();
    }
  } else {
    m_currentAsyncCreation.emplace_back();
  }
}

void V8Debugger::asyncTaskFinishedForStack(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
  // We could start instrumenting half way and the stack is empty.
  if (!m_currentTasks.size()) return;
  DCHECK(m_currentTasks.back() == task);
  m_currentTasks.pop_back();

  DCHECK(m_currentAsyncParent.size() == m_currentAsyncCreation.size());
  m_currentAsyncParent.pop_back();
  m_currentAsyncCreation.pop_back();

  if (m_recurringTasks.find(task) == m_recurringTasks.end()) {
    asyncTaskCanceledForStack(task);
  }
}

void V8Debugger::asyncTaskCandidateForStepping(void* task) {
  if (!m_stepIntoAsyncCallback) return;
  DCHECK(m_targetContextGroupId);
  if (currentContextGroupId() != m_targetContextGroupId) return;
  m_taskWithScheduledBreak = task;
  v8::debug::ClearStepping(m_isolate);
  m_stepIntoAsyncCallback->sendSuccess();
  m_stepIntoAsyncCallback.reset();
}

void V8Debugger::asyncTaskStartedForStepping(void* task) {
  if (m_breakRequested) return;
  if (task != m_taskWithScheduledBreak) return;
  v8::debug::DebugBreak(m_isolate);
}

void V8Debugger::asyncTaskFinishedForStepping(void* task) {
  if (task != m_taskWithScheduledBreak) return;
  m_taskWithScheduledBreak = nullptr;
  if (m_breakRequested) return;
  v8::debug::CancelDebugBreak(m_isolate);
}

void V8Debugger::asyncTaskCanceledForStepping(void* task) {
  if (task != m_taskWithScheduledBreak) return;
  m_taskWithScheduledBreak = nullptr;
}

void V8Debugger::allAsyncTasksCanceled() {
  m_asyncTaskStacks.clear();
  m_recurringTasks.clear();
  m_currentAsyncParent.clear();
  m_currentAsyncCreation.clear();
  m_currentTasks.clear();
  m_parentTask.clear();
  m_asyncTaskCreationStacks.clear();

  m_framesCache.clear();
  m_allAsyncStacks.clear();
  m_asyncStacksCount = 0;
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
  int contextGroupId = currentContextGroupId();
  if (!contextGroupId) return nullptr;

  int stackSize = 1;
  if (fullStack || m_inspector->enabledRuntimeAgentForGroup(contextGroupId)) {
    stackSize = V8StackTraceImpl::maxCallStackSizeToCapture;
  }
  return V8StackTraceImpl::capture(this, contextGroupId, stackSize);
}

int V8Debugger::currentContextGroupId() {
  if (!m_isolate->InContext()) return 0;
  return m_inspector->contextGroupId(m_isolate->GetCurrentContext());
}

void V8Debugger::collectOldAsyncStacksIfNeeded() {
  if (m_asyncStacksCount <= m_maxAsyncCallStacks) return;
  int halfOfLimitRoundedUp =
      m_maxAsyncCallStacks / 2 + m_maxAsyncCallStacks % 2;
  while (m_asyncStacksCount > halfOfLimitRoundedUp) {
    m_allAsyncStacks.pop_front();
    --m_asyncStacksCount;
  }
  cleanupExpiredWeakPointers(m_asyncTaskStacks);
  cleanupExpiredWeakPointers(m_asyncTaskCreationStacks);
  for (auto it = m_recurringTasks.begin(); it != m_recurringTasks.end();) {
    if (m_asyncTaskStacks.find(*it) == m_asyncTaskStacks.end()) {
      it = m_recurringTasks.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_parentTask.begin(); it != m_parentTask.end();) {
    if (m_asyncTaskCreationStacks.find(it->second) ==
            m_asyncTaskCreationStacks.end() &&
        m_asyncTaskStacks.find(it->second) == m_asyncTaskStacks.end()) {
      it = m_parentTask.erase(it);
    } else {
      ++it;
    }
  }
  cleanupExpiredWeakPointers(m_framesCache);
}

std::shared_ptr<StackFrame> V8Debugger::symbolize(
    v8::Local<v8::StackFrame> v8Frame) {
  auto it = m_framesCache.end();
  int frameId = 0;
  if (m_maxAsyncCallStackDepth) {
    frameId = v8::debug::GetStackFrameId(v8Frame);
    it = m_framesCache.find(frameId);
  }
  if (it != m_framesCache.end() && it->second.lock()) return it->second.lock();
  std::shared_ptr<StackFrame> frame(new StackFrame(v8Frame));
  // TODO(clemensh): Figure out a way to do this translation only right before
  // sending the stack trace over wire.
  if (v8Frame->IsWasm()) frame->translate(&m_wasmTranslation);
  if (m_maxAsyncCallStackDepth) {
    m_framesCache[frameId] = frame;
  }
  return frame;
}

void V8Debugger::setMaxAsyncTaskStacksForTest(int limit) {
  m_maxAsyncCallStacks = 0;
  collectOldAsyncStacksIfNeeded();
  m_maxAsyncCallStacks = limit;
}

void V8Debugger::dumpAsyncTaskStacksStateForTest() {
  fprintf(stdout, "Async stacks count: %d\n", m_asyncStacksCount);
  fprintf(stdout, "Scheduled async tasks: %zu\n", m_asyncTaskStacks.size());
  fprintf(stdout, "Created async tasks: %zu\n",
          m_asyncTaskCreationStacks.size());
  fprintf(stdout, "Async tasks with parent: %zu\n", m_parentTask.size());
  fprintf(stdout, "Recurring async tasks: %zu\n", m_recurringTasks.size());
  fprintf(stdout, "\n");
}

}  // namespace v8_inspector
