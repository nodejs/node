// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger.h"

#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-internal-value-type.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"
#include "src/inspector/v8-value-utils.h"

#include "include/v8-util.h"

namespace v8_inspector {

namespace {

static const int kMaxAsyncTaskStacks = 128 * 1024;
static const int kNoBreakpointId = 0;

v8::MaybeLocal<v8::Array> collectionsEntries(v8::Local<v8::Context> context,
                                             v8::Local<v8::Value> value) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Array> entries;
  bool isKeyValue = false;
  if (!value->IsObject() ||
      !value.As<v8::Object>()->PreviewEntries(&isKeyValue).ToLocal(&entries)) {
    return v8::MaybeLocal<v8::Array>();
  }

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

class MatchPrototypePredicate : public v8::debug::QueryObjectPredicate {
 public:
  MatchPrototypePredicate(V8InspectorImpl* inspector,
                          v8::Local<v8::Context> context,
                          v8::Local<v8::Object> prototype)
      : m_inspector(inspector), m_context(context), m_prototype(prototype) {}

  bool Filter(v8::Local<v8::Object> object) override {
    v8::Local<v8::Context> objectContext = object->CreationContext();
    if (objectContext != m_context) return false;
    if (!m_inspector->client()->isInspectableHeapObject(object)) return false;
    // Get prototype chain for current object until first visited prototype.
    for (v8::Local<v8::Value> prototype = object->GetPrototype();
         prototype->IsObject();
         prototype = prototype.As<v8::Object>()->GetPrototype()) {
      if (m_prototype == prototype) return true;
    }
    return false;
  }

 private:
  V8InspectorImpl* m_inspector;
  v8::Local<v8::Context> m_context;
  v8::Local<v8::Value> m_prototype;
};
}  // namespace

V8Debugger::V8Debugger(v8::Isolate* isolate, V8InspectorImpl* inspector)
    : m_isolate(isolate),
      m_inspector(inspector),
      m_enableCount(0),
      m_ignoreScriptParsedEventsCounter(0),
      m_continueToLocationBreakpointId(kNoBreakpointId),
      m_maxAsyncCallStacks(kMaxAsyncTaskStacks),
      m_maxAsyncCallStackDepth(0),
      m_pauseOnExceptionsState(v8::debug::NoBreakOnException),
      m_wasmTranslation(isolate) {}

V8Debugger::~V8Debugger() {
  m_isolate->RemoveCallCompletedCallback(
      &V8Debugger::terminateExecutionCompletedCallback);
  m_isolate->RemoveMicrotasksCompletedCallback(
      &V8Debugger::terminateExecutionCompletedCallback);
}

void V8Debugger::enable() {
  if (m_enableCount++) return;
  v8::HandleScope scope(m_isolate);
  v8::debug::SetDebugDelegate(m_isolate, this);
  m_isolate->AddNearHeapLimitCallback(&V8Debugger::nearHeapLimitCallback, this);
  v8::debug::ChangeBreakOnException(m_isolate, v8::debug::NoBreakOnException);
  m_pauseOnExceptionsState = v8::debug::NoBreakOnException;
}

void V8Debugger::disable() {
  if (isPaused()) {
    bool scheduledOOMBreak = m_scheduledOOMBreak;
    bool hasAgentAcceptsPause = false;
    m_inspector->forEachSession(
        m_pausedContextGroupId, [&scheduledOOMBreak, &hasAgentAcceptsPause](
                                    V8InspectorSessionImpl* session) {
          if (session->debuggerAgent()->acceptsPause(scheduledOOMBreak)) {
            hasAgentAcceptsPause = true;
          }
        });
    if (!hasAgentAcceptsPause) m_inspector->client()->quitMessageLoopOnPause();
  }
  if (--m_enableCount) return;
  clearContinueToLocation();
  m_taskWithScheduledBreak = nullptr;
  m_taskWithScheduledBreakDebuggerId = String16();
  m_pauseOnAsyncCall = false;
  m_wasmTranslation.Clear();
  v8::debug::SetDebugDelegate(m_isolate, nullptr);
  m_isolate->RemoveNearHeapLimitCallback(&V8Debugger::nearHeapLimitCallback,
                                         m_originalHeapLimit);
  m_originalHeapLimit = 0;
}

bool V8Debugger::isPausedInContextGroup(int contextGroupId) const {
  return isPaused() && m_pausedContextGroupId == contextGroupId;
}

bool V8Debugger::enabled() const { return m_enableCount > 0; }

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
      result.push_back(V8DebuggerScript::Create(m_isolate, script, false,
                                                m_inspector->client()));
      continue;
    }
    int contextId;
    if (!script->ContextId().To(&contextId)) continue;
    if (m_inspector->contextGroupId(contextId) != contextGroupId) continue;
    result.push_back(V8DebuggerScript::Create(m_isolate, script, false,
                                              m_inspector->client()));
  }
}

void V8Debugger::setBreakpointsActive(bool active) {
  if (!enabled()) {
    UNREACHABLE();
    return;
  }
  m_breakpointsActiveCount += active ? 1 : -1;
  v8::debug::SetBreakPointsActive(m_isolate, m_breakpointsActiveCount);
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

void V8Debugger::setPauseOnNextCall(bool pause, int targetContextGroupId) {
  if (isPaused()) return;
  DCHECK(targetContextGroupId);
  if (!pause && m_targetContextGroupId &&
      m_targetContextGroupId != targetContextGroupId) {
    return;
  }
  m_targetContextGroupId = targetContextGroupId;
  m_breakRequested = pause;
  if (pause)
    v8::debug::SetBreakOnNextFunctionCall(m_isolate);
  else
    v8::debug::ClearBreakOnNextFunctionCall(m_isolate);
}

bool V8Debugger::canBreakProgram() {
  return !v8::debug::AllFramesOnStackAreBlackboxed(m_isolate);
}

void V8Debugger::breakProgram(int targetContextGroupId) {
  DCHECK(canBreakProgram());
  // Don't allow nested breaks.
  if (isPaused()) return;
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::BreakRightNow(m_isolate);
}

void V8Debugger::interruptAndBreak(int targetContextGroupId) {
  // Don't allow nested breaks.
  if (isPaused()) return;
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  m_isolate->RequestInterrupt(
      [](v8::Isolate* isolate, void*) { v8::debug::BreakRightNow(isolate); },
      nullptr);
}

void V8Debugger::continueProgram(int targetContextGroupId) {
  if (m_pausedContextGroupId != targetContextGroupId) return;
  if (isPaused()) m_inspector->client()->quitMessageLoopOnPause();
}

void V8Debugger::breakProgramOnAssert(int targetContextGroupId) {
  if (!enabled()) return;
  if (m_pauseOnExceptionsState == v8::debug::NoBreakOnException) return;
  // Don't allow nested breaks.
  if (isPaused()) return;
  if (!canBreakProgram()) return;
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  m_scheduledAssertBreak = true;
  v8::debug::BreakRightNow(m_isolate);
}

void V8Debugger::stepIntoStatement(int targetContextGroupId,
                                   bool breakOnAsyncCall) {
  DCHECK(isPaused());
  DCHECK(targetContextGroupId);
  if (asyncStepOutOfFunction(targetContextGroupId, true)) return;
  m_targetContextGroupId = targetContextGroupId;
  m_pauseOnAsyncCall = breakOnAsyncCall;
  v8::debug::PrepareStep(m_isolate, v8::debug::StepIn);
  continueProgram(targetContextGroupId);
}

void V8Debugger::stepOverStatement(int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(targetContextGroupId);
  if (asyncStepOutOfFunction(targetContextGroupId, true)) return;
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::PrepareStep(m_isolate, v8::debug::StepNext);
  continueProgram(targetContextGroupId);
}

void V8Debugger::stepOutOfFunction(int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(targetContextGroupId);
  if (asyncStepOutOfFunction(targetContextGroupId, false)) return;
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::PrepareStep(m_isolate, v8::debug::StepOut);
  continueProgram(targetContextGroupId);
}

bool V8Debugger::asyncStepOutOfFunction(int targetContextGroupId,
                                        bool onlyAtReturn) {
  auto iterator = v8::debug::StackTraceIterator::Create(m_isolate);
  DCHECK(!iterator->Done());
  bool atReturn = !iterator->GetReturnValue().IsEmpty();
  iterator->Advance();
  // Synchronous stack has more then one frame.
  if (!iterator->Done()) return false;
  // There is only one synchronous frame but we are not at return position and
  // user requests stepOver or stepInto.
  if (onlyAtReturn && !atReturn) return false;
  // If we are inside async function, current async parent was captured when
  // async function was suspended first time and we install that stack as
  // current before resume async function. So it represents current async
  // function.
  auto current = currentAsyncParent();
  if (!current) return false;
  // Lookup for parent async function.
  auto parent = current->parent();
  if (parent.expired()) return false;
  // Parent async stack will have suspended task id iff callee async function
  // is awaiting current async function. We can make stepOut there only in this
  // case.
  void* parentTask =
      std::shared_ptr<AsyncStackTrace>(parent)->suspendedTaskId();
  if (!parentTask) return false;
  pauseOnAsyncCall(targetContextGroupId,
                   reinterpret_cast<uintptr_t>(parentTask), String16());
  continueProgram(targetContextGroupId);
  return true;
}

void V8Debugger::scheduleStepIntoAsync(
    std::unique_ptr<ScheduleStepIntoAsyncCallback> callback,
    int targetContextGroupId) {
  DCHECK(isPaused());
  DCHECK(targetContextGroupId);
  if (m_stepIntoAsyncCallback) {
    m_stepIntoAsyncCallback->sendFailure(Response::Error(
        "Current scheduled step into async was overriden with new one."));
  }
  m_targetContextGroupId = targetContextGroupId;
  m_stepIntoAsyncCallback = std::move(callback);
}

void V8Debugger::pauseOnAsyncCall(int targetContextGroupId, uintptr_t task,
                                  const String16& debuggerId) {
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;

  m_taskWithScheduledBreak = reinterpret_cast<void*>(task);
  m_taskWithScheduledBreakDebuggerId = debuggerId;
}

void V8Debugger::terminateExecution(
    std::unique_ptr<TerminateExecutionCallback> callback) {
  if (m_terminateExecutionCallback) {
    if (callback) {
      callback->sendFailure(
          Response::Error("There is current termination request in progress"));
    }
    return;
  }
  m_terminateExecutionCallback = std::move(callback);
  m_isolate->AddCallCompletedCallback(
      &V8Debugger::terminateExecutionCompletedCallback);
  m_isolate->AddMicrotasksCompletedCallback(
      &V8Debugger::terminateExecutionCompletedCallback);
  m_isolate->TerminateExecution();
}

void V8Debugger::terminateExecutionCompletedCallback(v8::Isolate* isolate) {
  isolate->RemoveCallCompletedCallback(
      &V8Debugger::terminateExecutionCompletedCallback);
  isolate->RemoveMicrotasksCompletedCallback(
      &V8Debugger::terminateExecutionCompletedCallback);
  V8InspectorImpl* inspector =
      static_cast<V8InspectorImpl*>(v8::debug::GetInspector(isolate));
  V8Debugger* debugger = inspector->debugger();
  debugger->m_isolate->CancelTerminateExecution();
  if (debugger->m_terminateExecutionCallback) {
    debugger->m_terminateExecutionCallback->sendSuccess();
    debugger->m_terminateExecutionCallback.reset();
  }
}

Response V8Debugger::continueToLocation(
    int targetContextGroupId, V8DebuggerScript* script,
    std::unique_ptr<protocol::Debugger::Location> location,
    const String16& targetCallFrames) {
  DCHECK(isPaused());
  DCHECK(targetContextGroupId);
  m_targetContextGroupId = targetContextGroupId;
  v8::debug::Location v8Location(location->getLineNumber(),
                                 location->getColumnNumber(0));
  if (script->setBreakpoint(String16(), &v8Location,
                            &m_continueToLocationBreakpointId)) {
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
  if (m_continueToLocationBreakpointId == kNoBreakpointId) return;
  v8::debug::RemoveBreakpoint(m_isolate, m_continueToLocationBreakpointId);
  m_continueToLocationBreakpointId = kNoBreakpointId;
  m_continueToLocationTargetCallFrames = String16();
  m_continueToLocationStack.reset();
}

void V8Debugger::handleProgramBreak(
    v8::Local<v8::Context> pausedContext, v8::Local<v8::Value> exception,
    const std::vector<v8::debug::BreakpointId>& breakpointIds,
    v8::debug::ExceptionType exceptionType, bool isUncaught) {
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
  m_pauseOnAsyncCall = false;
  m_taskWithScheduledBreak = nullptr;
  m_taskWithScheduledBreakDebuggerId = String16();

  bool scheduledOOMBreak = m_scheduledOOMBreak;
  bool scheduledAssertBreak = m_scheduledAssertBreak;
  bool hasAgents = false;
  m_inspector->forEachSession(
      contextGroupId,
      [&scheduledOOMBreak, &hasAgents](V8InspectorSessionImpl* session) {
        if (session->debuggerAgent()->acceptsPause(scheduledOOMBreak))
          hasAgents = true;
      });
  if (!hasAgents) return;

  if (breakpointIds.size() == 1 &&
      breakpointIds[0] == m_continueToLocationBreakpointId) {
    v8::Context::Scope contextScope(pausedContext);
    if (!shouldContinueToCurrentLocation()) return;
  }
  clearContinueToLocation();

  DCHECK(contextGroupId);
  m_pausedContextGroupId = contextGroupId;

  m_inspector->forEachSession(
      contextGroupId, [&pausedContext, &exception, &breakpointIds,
                       &exceptionType, &isUncaught, &scheduledOOMBreak,
                       &scheduledAssertBreak](V8InspectorSessionImpl* session) {
        if (session->debuggerAgent()->acceptsPause(scheduledOOMBreak)) {
          session->debuggerAgent()->didPause(
              InspectedContext::contextId(pausedContext), exception,
              breakpointIds, exceptionType, isUncaught, scheduledOOMBreak,
              scheduledAssertBreak);
        }
      });
  {
    v8::Context::Scope scope(pausedContext);
    m_inspector->client()->runMessageLoopOnPause(contextGroupId);
    m_pausedContextGroupId = 0;
  }
  m_inspector->forEachSession(contextGroupId,
                              [](V8InspectorSessionImpl* session) {
                                if (session->debuggerAgent()->enabled())
                                  session->debuggerAgent()->didContinue();
                              });

  if (m_scheduledOOMBreak) m_isolate->RestoreOriginalHeapLimit();
  m_scheduledOOMBreak = false;
  m_scheduledAssertBreak = false;
}

namespace {

size_t HeapLimitForDebugging(size_t initial_heap_limit) {
  const size_t kDebugHeapSizeFactor = 4;
  size_t max_limit = std::numeric_limits<size_t>::max() / 4;
  return std::min(max_limit, initial_heap_limit * kDebugHeapSizeFactor);
}

}  // anonymous namespace

size_t V8Debugger::nearHeapLimitCallback(void* data, size_t current_heap_limit,
                                         size_t initial_heap_limit) {
  V8Debugger* thisPtr = static_cast<V8Debugger*>(data);
  thisPtr->m_originalHeapLimit = current_heap_limit;
  thisPtr->m_scheduledOOMBreak = true;
  v8::Local<v8::Context> context = thisPtr->m_isolate->GetEnteredContext();
  thisPtr->m_targetContextGroupId =
      context.IsEmpty() ? 0 : thisPtr->m_inspector->contextGroupId(context);
  thisPtr->m_isolate->RequestInterrupt(
      [](v8::Isolate* isolate, void*) { v8::debug::BreakRightNow(isolate); },
      nullptr);
  return HeapLimitForDebugging(initial_heap_limit);
}

void V8Debugger::ScriptCompiled(v8::Local<v8::debug::Script> script,
                                bool is_live_edited, bool has_compile_error) {
  int contextId;
  if (!script->ContextId().To(&contextId)) return;
  if (script->IsWasm() && script->SourceMappingURL().IsEmpty()) {
    WasmTranslation* wasmTranslation = &m_wasmTranslation;
    m_inspector->forEachSession(
        m_inspector->contextGroupId(contextId),
        [&script, &wasmTranslation](V8InspectorSessionImpl* session) {
          if (!session->debuggerAgent()->enabled()) return;
          wasmTranslation->AddScript(script.As<v8::debug::WasmScript>(),
                                     session->debuggerAgent());
        });
  } else if (m_ignoreScriptParsedEventsCounter == 0) {
    v8::Isolate* isolate = m_isolate;
    V8InspectorClient* client = m_inspector->client();
    m_inspector->forEachSession(
        m_inspector->contextGroupId(contextId),
        [&isolate, &script, &has_compile_error, &is_live_edited,
         &client](V8InspectorSessionImpl* session) {
          if (!session->debuggerAgent()->enabled()) return;
          session->debuggerAgent()->didParseSource(
              V8DebuggerScript::Create(isolate, script, is_live_edited, client),
              !has_compile_error);
        });
  }
}

void V8Debugger::BreakProgramRequested(
    v8::Local<v8::Context> pausedContext,
    const std::vector<v8::debug::BreakpointId>& break_points_hit) {
  handleProgramBreak(pausedContext, v8::Local<v8::Value>(), break_points_hit);
}

void V8Debugger::ExceptionThrown(v8::Local<v8::Context> pausedContext,
                                 v8::Local<v8::Value> exception,
                                 v8::Local<v8::Value> promise, bool isUncaught,
                                 v8::debug::ExceptionType exceptionType) {
  std::vector<v8::debug::BreakpointId> break_points_hit;
  handleProgramBreak(pausedContext, exception, break_points_hit, exceptionType,
                     isUncaught);
}

bool V8Debugger::IsFunctionBlackboxed(v8::Local<v8::debug::Script> script,
                                      const v8::debug::Location& start,
                                      const v8::debug::Location& end) {
  int contextId;
  if (!script->ContextId().To(&contextId)) return false;
  bool hasAgents = false;
  bool allBlackboxed = true;
  String16 scriptId = String16::fromInteger(script->Id());
  m_inspector->forEachSession(
      m_inspector->contextGroupId(contextId),
      [&hasAgents, &allBlackboxed, &scriptId, &start,
       &end](V8InspectorSessionImpl* session) {
        V8DebuggerAgentImpl* agent = session->debuggerAgent();
        if (!agent->enabled()) return;
        hasAgents = true;
        allBlackboxed &= agent->isFunctionBlackboxed(scriptId, start, end);
      });
  return hasAgents && allBlackboxed;
}

void V8Debugger::AsyncEventOccurred(v8::debug::DebugAsyncActionType type,
                                    int id, bool isBlackboxed) {
  // Async task events from Promises are given misaligned pointers to prevent
  // from overlapping with other Blink task identifiers.
  void* task = reinterpret_cast<void*>(id * 2 + 1);
  switch (type) {
    case v8::debug::kDebugPromiseThen:
      asyncTaskScheduledForStack("Promise.then", task, false);
      if (!isBlackboxed) asyncTaskCandidateForStepping(task, true);
      break;
    case v8::debug::kDebugPromiseCatch:
      asyncTaskScheduledForStack("Promise.catch", task, false);
      if (!isBlackboxed) asyncTaskCandidateForStepping(task, true);
      break;
    case v8::debug::kDebugPromiseFinally:
      asyncTaskScheduledForStack("Promise.finally", task, false);
      if (!isBlackboxed) asyncTaskCandidateForStepping(task, true);
      break;
    case v8::debug::kDebugWillHandle:
      asyncTaskStartedForStack(task);
      asyncTaskStartedForStepping(task);
      break;
    case v8::debug::kDebugDidHandle:
      asyncTaskFinishedForStack(task);
      asyncTaskFinishedForStepping(task);
      break;
    case v8::debug::kAsyncFunctionSuspended: {
      if (m_asyncTaskStacks.find(task) == m_asyncTaskStacks.end()) {
        asyncTaskScheduledForStack("async function", task, true);
      }
      auto stackIt = m_asyncTaskStacks.find(task);
      if (stackIt != m_asyncTaskStacks.end() && !stackIt->second.expired()) {
        std::shared_ptr<AsyncStackTrace> stack(stackIt->second);
        stack->setSuspendedTaskId(task);
      }
      break;
    }
    case v8::debug::kAsyncFunctionFinished:
      asyncTaskCanceledForStack(task);
      break;
  }
}

std::shared_ptr<AsyncStackTrace> V8Debugger::currentAsyncParent() {
  return m_currentAsyncParent.empty() ? nullptr : m_currentAsyncParent.back();
}

V8StackTraceId V8Debugger::currentExternalParent() {
  return m_currentExternalParent.empty() ? V8StackTraceId()
                                         : m_currentExternalParent.back();
}

v8::MaybeLocal<v8::Value> V8Debugger::getTargetScopes(
    v8::Local<v8::Context> context, v8::Local<v8::Value> value,
    ScopeTargetKind kind) {
  v8::Local<v8::Value> scopesValue;
  std::unique_ptr<v8::debug::ScopeIterator> iterator;
  switch (kind) {
    case FUNCTION:
      iterator = v8::debug::ScopeIterator::CreateForFunction(
          m_isolate, v8::Local<v8::Function>::Cast(value));
      break;
    case GENERATOR:
      v8::Local<v8::debug::GeneratorObject> generatorObject =
          v8::debug::GeneratorObject::Cast(value);
      if (!generatorObject->IsSuspended()) return v8::MaybeLocal<v8::Value>();

      iterator = v8::debug::ScopeIterator::CreateForGeneratorObject(
          m_isolate, v8::Local<v8::Object>::Cast(value));
      break;
  }
  if (!iterator) return v8::MaybeLocal<v8::Value>();
  v8::Local<v8::Array> result = v8::Array::New(m_isolate);
  if (!result->SetPrototype(context, v8::Null(m_isolate)).FromMaybe(false)) {
    return v8::MaybeLocal<v8::Value>();
  }

  for (; !iterator->Done(); iterator->Advance()) {
    v8::Local<v8::Object> scope = v8::Object::New(m_isolate);
    if (!markAsInternal(context, scope, V8InternalValueType::kScope)) {
      return v8::MaybeLocal<v8::Value>();
    }
    String16 type = v8_inspector::scopeType(iterator->GetType());
    String16 name;
    v8::Local<v8::Value> maybe_name = iterator->GetFunctionDebugName();
    if (!maybe_name->IsUndefined()) {
      name = toProtocolStringWithTypeCheck(m_isolate, maybe_name);
    }
    v8::Local<v8::Object> object = iterator->GetObject();
    createDataProperty(context, scope,
                       toV8StringInternalized(m_isolate, "type"),
                       toV8String(m_isolate, type));
    createDataProperty(context, scope,
                       toV8StringInternalized(m_isolate, "name"),
                       toV8String(m_isolate, name));
    createDataProperty(context, scope,
                       toV8StringInternalized(m_isolate, "object"), object);
    createDataProperty(context, result, result->Length(), scope);
  }
  if (!markAsInternal(context, v8::Local<v8::Array>::Cast(result),
                      V8InternalValueType::kScopeList))
    return v8::MaybeLocal<v8::Value>();
  return result;
}

v8::MaybeLocal<v8::Value> V8Debugger::functionScopes(
    v8::Local<v8::Context> context, v8::Local<v8::Function> function) {
  return getTargetScopes(context, function, FUNCTION);
}

v8::MaybeLocal<v8::Value> V8Debugger::generatorScopes(
    v8::Local<v8::Context> context, v8::Local<v8::Value> generator) {
  return getTargetScopes(context, generator, GENERATOR);
}

v8::MaybeLocal<v8::Uint32> V8Debugger::stableObjectId(
    v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
  DCHECK(value->IsObject());
  if (m_stableObjectId.IsEmpty()) {
    m_stableObjectId.Reset(m_isolate, v8::debug::WeakMap::New(m_isolate));
  }
  v8::Local<v8::debug::WeakMap> stableObjectId =
      m_stableObjectId.Get(m_isolate);
  v8::Local<v8::Value> idValue;
  if (!stableObjectId->Get(context, value).ToLocal(&idValue) ||
      !idValue->IsUint32()) {
    idValue = v8::Integer::NewFromUnsigned(m_isolate, ++m_lastStableObjectId);
    stableObjectId->Set(context, value, idValue).ToLocalChecked();
  }
  return idValue.As<v8::Uint32>();
}

v8::MaybeLocal<v8::Array> V8Debugger::internalProperties(
    v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
  v8::Local<v8::Array> properties;
  if (!v8::debug::GetInternalProperties(m_isolate, value).ToLocal(&properties))
    return v8::MaybeLocal<v8::Array>();
  if (value->IsObject()) {
    v8::Local<v8::Uint32> id;
    if (stableObjectId(context, value).ToLocal(&id)) {
      createDataProperty(
          context, properties, properties->Length(),
          toV8StringInternalized(m_isolate, "[[StableObjectId]]"));
      createDataProperty(context, properties, properties->Length(), id);
    }
  }
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
    v8::Local<v8::Value> scopes;
    if (generatorScopes(context, value).ToLocal(&scopes)) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[Scopes]]"));
      createDataProperty(context, properties, properties->Length(), scopes);
    }
  }
  if (value->IsFunction()) {
    v8::Local<v8::Function> function = value.As<v8::Function>();
    v8::Local<v8::Value> scopes;
    if (functionScopes(context, function).ToLocal(&scopes)) {
      createDataProperty(context, properties, properties->Length(),
                         toV8StringInternalized(m_isolate, "[[Scopes]]"));
      createDataProperty(context, properties, properties->Length(), scopes);
    }
  }
  return properties;
}

v8::Local<v8::Array> V8Debugger::queryObjects(v8::Local<v8::Context> context,
                                              v8::Local<v8::Object> prototype) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::PersistentValueVector<v8::Object> v8Objects(isolate);
  MatchPrototypePredicate predicate(m_inspector, context, prototype);
  v8::debug::QueryObjects(context, &predicate, &v8Objects);

  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Array> resultArray = v8::Array::New(
      m_inspector->isolate(), static_cast<int>(v8Objects.Size()));
  for (size_t i = 0; i < v8Objects.Size(); ++i) {
    createDataProperty(context, resultArray, static_cast<int>(i),
                       v8Objects.Get(i));
  }
  return resultArray;
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
  // TODO(dgozman): ideally, this should be per context group.
  m_maxAsyncCallStackDepth = maxAsyncCallStackDepth;
  m_inspector->client()->maxAsyncCallStackDepthChanged(
      m_maxAsyncCallStackDepth);
  if (!maxAsyncCallStackDepth) allAsyncTasksCanceled();
  v8::debug::SetAsyncEventDelegate(m_isolate,
                                   maxAsyncCallStackDepth ? this : nullptr);
}

std::shared_ptr<AsyncStackTrace> V8Debugger::stackTraceFor(
    int contextGroupId, const V8StackTraceId& id) {
  if (debuggerIdFor(contextGroupId) != id.debugger_id) return nullptr;
  auto it = m_storedStackTraces.find(id.id);
  if (it == m_storedStackTraces.end()) return nullptr;
  return it->second.lock();
}

V8StackTraceId V8Debugger::storeCurrentStackTrace(
    const StringView& description) {
  if (!m_maxAsyncCallStackDepth) return V8StackTraceId();

  v8::HandleScope scope(m_isolate);
  int contextGroupId = currentContextGroupId();
  if (!contextGroupId) return V8StackTraceId();

  std::shared_ptr<AsyncStackTrace> asyncStack =
      AsyncStackTrace::capture(this, contextGroupId, toString16(description),
                               V8StackTraceImpl::maxCallStackSizeToCapture);
  if (!asyncStack) return V8StackTraceId();

  uintptr_t id = AsyncStackTrace::store(this, asyncStack);

  m_allAsyncStacks.push_back(std::move(asyncStack));
  ++m_asyncStacksCount;
  collectOldAsyncStacksIfNeeded();

  asyncTaskCandidateForStepping(reinterpret_cast<void*>(id), false);

  return V8StackTraceId(id, debuggerIdFor(contextGroupId));
}

uintptr_t V8Debugger::storeStackTrace(
    std::shared_ptr<AsyncStackTrace> asyncStack) {
  uintptr_t id = ++m_lastStackTraceId;
  m_storedStackTraces[id] = asyncStack;
  return id;
}

void V8Debugger::externalAsyncTaskStarted(const V8StackTraceId& parent) {
  if (!m_maxAsyncCallStackDepth || parent.IsInvalid()) return;
  m_currentExternalParent.push_back(parent);
  m_currentAsyncParent.emplace_back();
  m_currentTasks.push_back(reinterpret_cast<void*>(parent.id));

  if (m_breakRequested) return;
  if (!m_taskWithScheduledBreakDebuggerId.isEmpty() &&
      reinterpret_cast<uintptr_t>(m_taskWithScheduledBreak) == parent.id &&
      m_taskWithScheduledBreakDebuggerId ==
          debuggerIdToString(parent.debugger_id)) {
    v8::debug::SetBreakOnNextFunctionCall(m_isolate);
  }
}

void V8Debugger::externalAsyncTaskFinished(const V8StackTraceId& parent) {
  if (!m_maxAsyncCallStackDepth || m_currentExternalParent.empty()) return;
  m_currentExternalParent.pop_back();
  m_currentAsyncParent.pop_back();
  DCHECK(m_currentTasks.back() == reinterpret_cast<void*>(parent.id));
  m_currentTasks.pop_back();

  if (m_taskWithScheduledBreakDebuggerId.isEmpty() ||
      reinterpret_cast<uintptr_t>(m_taskWithScheduledBreak) != parent.id ||
      m_taskWithScheduledBreakDebuggerId !=
          debuggerIdToString(parent.debugger_id)) {
    return;
  }
  m_taskWithScheduledBreak = nullptr;
  m_taskWithScheduledBreakDebuggerId = String16();
  if (m_breakRequested) return;
  v8::debug::ClearBreakOnNextFunctionCall(m_isolate);
}

void V8Debugger::asyncTaskScheduled(const StringView& taskName, void* task,
                                    bool recurring) {
  asyncTaskScheduledForStack(toString16(taskName), task, recurring);
  asyncTaskCandidateForStepping(task, true);
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
  asyncTaskFinishedForStepping(task);
  asyncTaskFinishedForStack(task);
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
  AsyncTaskToStackTrace::iterator stackIt = m_asyncTaskStacks.find(task);
  if (stackIt != m_asyncTaskStacks.end() && !stackIt->second.expired()) {
    std::shared_ptr<AsyncStackTrace> stack(stackIt->second);
    stack->setSuspendedTaskId(nullptr);
    m_currentAsyncParent.push_back(stack);
  } else {
    m_currentAsyncParent.emplace_back();
  }
  m_currentExternalParent.emplace_back();
}

void V8Debugger::asyncTaskFinishedForStack(void* task) {
  if (!m_maxAsyncCallStackDepth) return;
  // We could start instrumenting half way and the stack is empty.
  if (!m_currentTasks.size()) return;
  DCHECK(m_currentTasks.back() == task);
  m_currentTasks.pop_back();

  m_currentAsyncParent.pop_back();
  m_currentExternalParent.pop_back();

  if (m_recurringTasks.find(task) == m_recurringTasks.end()) {
    asyncTaskCanceledForStack(task);
  }
}

void V8Debugger::asyncTaskCandidateForStepping(void* task, bool isLocal) {
  int contextGroupId = currentContextGroupId();
  if (m_pauseOnAsyncCall && contextGroupId) {
    if (isLocal) {
      m_scheduledAsyncCall = v8_inspector::V8StackTraceId(
          reinterpret_cast<uintptr_t>(task), std::make_pair(0, 0));
    } else {
      m_scheduledAsyncCall = v8_inspector::V8StackTraceId(
          reinterpret_cast<uintptr_t>(task), debuggerIdFor(contextGroupId));
    }
    breakProgram(m_targetContextGroupId);
    m_scheduledAsyncCall = v8_inspector::V8StackTraceId();
    return;
  }
  if (!m_stepIntoAsyncCallback) return;
  DCHECK(m_targetContextGroupId);
  if (contextGroupId != m_targetContextGroupId) return;
  m_taskWithScheduledBreak = task;
  v8::debug::ClearStepping(m_isolate);
  m_stepIntoAsyncCallback->sendSuccess();
  m_stepIntoAsyncCallback.reset();
}

void V8Debugger::asyncTaskStartedForStepping(void* task) {
  if (m_breakRequested) return;
  // TODO(kozyatinskiy): we should search task in async chain to support
  // blackboxing.
  if (m_taskWithScheduledBreakDebuggerId.isEmpty() &&
      task == m_taskWithScheduledBreak) {
    v8::debug::SetBreakOnNextFunctionCall(m_isolate);
  }
}

void V8Debugger::asyncTaskFinishedForStepping(void* task) {
  if (!m_taskWithScheduledBreakDebuggerId.isEmpty() ||
      task != m_taskWithScheduledBreak) {
    return;
  }
  m_taskWithScheduledBreak = nullptr;
  if (m_breakRequested) return;
  v8::debug::ClearBreakOnNextFunctionCall(m_isolate);
}

void V8Debugger::asyncTaskCanceledForStepping(void* task) {
  if (!m_taskWithScheduledBreakDebuggerId.isEmpty() ||
      task != m_taskWithScheduledBreak)
    return;
  m_taskWithScheduledBreak = nullptr;
}

void V8Debugger::allAsyncTasksCanceled() {
  m_asyncTaskStacks.clear();
  m_recurringTasks.clear();
  m_currentAsyncParent.clear();
  m_currentExternalParent.clear();
  m_currentTasks.clear();

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
  if (fullStack) {
    stackSize = V8StackTraceImpl::maxCallStackSizeToCapture;
  } else {
    m_inspector->forEachSession(
        contextGroupId, [&stackSize](V8InspectorSessionImpl* session) {
          if (session->runtimeAgent()->enabled())
            stackSize = V8StackTraceImpl::maxCallStackSizeToCapture;
        });
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
  cleanupExpiredWeakPointers(m_storedStackTraces);
  for (auto it = m_recurringTasks.begin(); it != m_recurringTasks.end();) {
    if (m_asyncTaskStacks.find(*it) == m_asyncTaskStacks.end()) {
      it = m_recurringTasks.erase(it);
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
  if (it != m_framesCache.end() && !it->second.expired()) {
    return std::shared_ptr<StackFrame>(it->second);
  }
  std::shared_ptr<StackFrame> frame(new StackFrame(isolate(), v8Frame));
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

std::pair<int64_t, int64_t> V8Debugger::debuggerIdFor(int contextGroupId) {
  auto it = m_contextGroupIdToDebuggerId.find(contextGroupId);
  if (it != m_contextGroupIdToDebuggerId.end()) return it->second;
  std::pair<int64_t, int64_t> debuggerId(
      v8::debug::GetNextRandomInt64(m_isolate),
      v8::debug::GetNextRandomInt64(m_isolate));
  if (!debuggerId.first && !debuggerId.second) ++debuggerId.first;
  m_contextGroupIdToDebuggerId.insert(
      it, std::make_pair(contextGroupId, debuggerId));
  m_serializedDebuggerIdToDebuggerId.insert(
      std::make_pair(debuggerIdToString(debuggerId), debuggerId));
  return debuggerId;
}

std::pair<int64_t, int64_t> V8Debugger::debuggerIdFor(
    const String16& serializedDebuggerId) {
  auto it = m_serializedDebuggerIdToDebuggerId.find(serializedDebuggerId);
  if (it != m_serializedDebuggerIdToDebuggerId.end()) return it->second;
  return std::make_pair(0, 0);
}

void V8Debugger::dumpAsyncTaskStacksStateForTest() {
  fprintf(stdout, "Async stacks count: %d\n", m_asyncStacksCount);
  fprintf(stdout, "Scheduled async tasks: %zu\n", m_asyncTaskStacks.size());
  fprintf(stdout, "Recurring async tasks: %zu\n", m_recurringTasks.size());
  fprintf(stdout, "\n");
}

}  // namespace v8_inspector
