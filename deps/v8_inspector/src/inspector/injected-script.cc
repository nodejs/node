/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "src/inspector/injected-script.h"

#include "src/inspector/injected-script-native.h"
#include "src/inspector/injected-script-source.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/remote-object-id.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-function-call.h"
#include "src/inspector/v8-injected-script-host.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"
#include "src/inspector/v8-value-copier.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

using protocol::Array;
using protocol::Runtime::PropertyDescriptor;
using protocol::Runtime::InternalPropertyDescriptor;
using protocol::Runtime::RemoteObject;
using protocol::Maybe;

static bool hasInternalError(ErrorString* errorString, bool hasError) {
  if (hasError) *errorString = "Internal error";
  return hasError;
}

std::unique_ptr<InjectedScript> InjectedScript::create(
    InspectedContext* inspectedContext) {
  v8::Isolate* isolate = inspectedContext->isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = inspectedContext->context();
  v8::Context::Scope scope(context);

  std::unique_ptr<InjectedScriptNative> injectedScriptNative(
      new InjectedScriptNative(isolate));
  v8::Local<v8::Object> scriptHostWrapper =
      V8InjectedScriptHost::create(context, inspectedContext->inspector());
  injectedScriptNative->setOnInjectedScriptHost(scriptHostWrapper);

  // Inject javascript into the context. The compiled script is supposed to
  // evaluate into
  // a single anonymous function(it's anonymous to avoid cluttering the global
  // object with
  // inspector's stuff) the function is called a few lines below with
  // InjectedScriptHost wrapper,
  // injected script id and explicit reference to the inspected global object.
  // The function is expected
  // to create and configure InjectedScript instance that is going to be used by
  // the inspector.
  String16 injectedScriptSource(
      reinterpret_cast<const char*>(InjectedScriptSource_js),
      sizeof(InjectedScriptSource_js));
  v8::Local<v8::Value> value;
  if (!inspectedContext->inspector()
           ->compileAndRunInternalScript(
               context, toV8String(isolate, injectedScriptSource))
           .ToLocal(&value))
    return nullptr;
  DCHECK(value->IsFunction());
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  v8::Local<v8::Object> windowGlobal = context->Global();
  v8::Local<v8::Value> info[] = {
      scriptHostWrapper, windowGlobal,
      v8::Number::New(isolate, inspectedContext->contextId())};
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> injectedScriptValue;
  if (!function->Call(context, windowGlobal, arraysize(info), info)
           .ToLocal(&injectedScriptValue))
    return nullptr;
  if (!injectedScriptValue->IsObject()) return nullptr;
  return wrapUnique(new InjectedScript(inspectedContext,
                                       injectedScriptValue.As<v8::Object>(),
                                       std::move(injectedScriptNative)));
}

InjectedScript::InjectedScript(
    InspectedContext* context, v8::Local<v8::Object> object,
    std::unique_ptr<InjectedScriptNative> injectedScriptNative)
    : m_context(context),
      m_value(context->isolate(), object),
      m_native(std::move(injectedScriptNative)) {}

InjectedScript::~InjectedScript() {}

void InjectedScript::getProperties(
    ErrorString* errorString, v8::Local<v8::Object> object,
    const String16& groupName, bool ownProperties, bool accessorPropertiesOnly,
    bool generatePreview,
    std::unique_ptr<Array<PropertyDescriptor>>* properties,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  v8::HandleScope handles(m_context->isolate());
  v8::Local<v8::Context> context = m_context->context();
  V8FunctionCall function(m_context->inspector(), m_context->context(),
                          v8Value(), "getProperties");
  function.appendArgument(object);
  function.appendArgument(groupName);
  function.appendArgument(ownProperties);
  function.appendArgument(accessorPropertiesOnly);
  function.appendArgument(generatePreview);

  v8::TryCatch tryCatch(m_context->isolate());
  v8::Local<v8::Value> resultValue = function.callWithoutExceptionHandling();
  if (tryCatch.HasCaught()) {
    *exceptionDetails = createExceptionDetails(errorString, tryCatch, groupName,
                                               generatePreview);
    // FIXME: make properties optional
    *properties = Array<PropertyDescriptor>::create();
    return;
  }
  if (hasInternalError(errorString, resultValue.IsEmpty())) return;
  std::unique_ptr<protocol::Value> protocolValue =
      toProtocolValue(errorString, context, resultValue);
  if (!protocolValue) return;
  protocol::ErrorSupport errors(errorString);
  std::unique_ptr<Array<PropertyDescriptor>> result =
      Array<PropertyDescriptor>::parse(protocolValue.get(), &errors);
  if (!hasInternalError(errorString, errors.hasErrors()))
    *properties = std::move(result);
}

void InjectedScript::releaseObject(const String16& objectId) {
  std::unique_ptr<protocol::Value> parsedObjectId =
      protocol::parseJSON(objectId);
  if (!parsedObjectId) return;
  protocol::DictionaryValue* object =
      protocol::DictionaryValue::cast(parsedObjectId.get());
  if (!object) return;
  int boundId = 0;
  if (!object->getInteger("id", &boundId)) return;
  m_native->unbind(boundId);
}

std::unique_ptr<protocol::Runtime::RemoteObject> InjectedScript::wrapObject(
    ErrorString* errorString, v8::Local<v8::Value> value,
    const String16& groupName, bool forceValueType,
    bool generatePreview) const {
  v8::HandleScope handles(m_context->isolate());
  v8::Local<v8::Value> wrappedObject;
  v8::Local<v8::Context> context = m_context->context();
  if (!wrapValue(errorString, value, groupName, forceValueType, generatePreview)
           .ToLocal(&wrappedObject))
    return nullptr;
  protocol::ErrorSupport errors;
  std::unique_ptr<protocol::Value> protocolValue =
      toProtocolValue(errorString, context, wrappedObject);
  if (!protocolValue) return nullptr;
  std::unique_ptr<protocol::Runtime::RemoteObject> remoteObject =
      protocol::Runtime::RemoteObject::parse(protocolValue.get(), &errors);
  if (!remoteObject) *errorString = errors.errors();
  return remoteObject;
}

bool InjectedScript::wrapObjectProperty(ErrorString* errorString,
                                        v8::Local<v8::Object> object,
                                        v8::Local<v8::Name> key,
                                        const String16& groupName,
                                        bool forceValueType,
                                        bool generatePreview) const {
  v8::Local<v8::Value> property;
  v8::Local<v8::Context> context = m_context->context();
  if (hasInternalError(errorString,
                       !object->Get(context, key).ToLocal(&property)))
    return false;
  v8::Local<v8::Value> wrappedProperty;
  if (!wrapValue(errorString, property, groupName, forceValueType,
                 generatePreview)
           .ToLocal(&wrappedProperty))
    return false;
  v8::Maybe<bool> success =
      createDataProperty(context, object, key, wrappedProperty);
  if (hasInternalError(errorString, success.IsNothing() || !success.FromJust()))
    return false;
  return true;
}

bool InjectedScript::wrapPropertyInArray(ErrorString* errorString,
                                         v8::Local<v8::Array> array,
                                         v8::Local<v8::String> property,
                                         const String16& groupName,
                                         bool forceValueType,
                                         bool generatePreview) const {
  V8FunctionCall function(m_context->inspector(), m_context->context(),
                          v8Value(), "wrapPropertyInArray");
  function.appendArgument(array);
  function.appendArgument(property);
  function.appendArgument(groupName);
  function.appendArgument(forceValueType);
  function.appendArgument(generatePreview);
  bool hadException = false;
  function.call(hadException);
  return !hasInternalError(errorString, hadException);
}

bool InjectedScript::wrapObjectsInArray(ErrorString* errorString,
                                        v8::Local<v8::Array> array,
                                        const String16& groupName,
                                        bool forceValueType,
                                        bool generatePreview) const {
  V8FunctionCall function(m_context->inspector(), m_context->context(),
                          v8Value(), "wrapObjectsInArray");
  function.appendArgument(array);
  function.appendArgument(groupName);
  function.appendArgument(forceValueType);
  function.appendArgument(generatePreview);
  bool hadException = false;
  function.call(hadException);
  return !hasInternalError(errorString, hadException);
}

v8::MaybeLocal<v8::Value> InjectedScript::wrapValue(
    ErrorString* errorString, v8::Local<v8::Value> value,
    const String16& groupName, bool forceValueType,
    bool generatePreview) const {
  V8FunctionCall function(m_context->inspector(), m_context->context(),
                          v8Value(), "wrapObject");
  function.appendArgument(value);
  function.appendArgument(groupName);
  function.appendArgument(forceValueType);
  function.appendArgument(generatePreview);
  bool hadException = false;
  v8::Local<v8::Value> r = function.call(hadException);
  if (hasInternalError(errorString, hadException || r.IsEmpty()))
    return v8::MaybeLocal<v8::Value>();
  return r;
}

std::unique_ptr<protocol::Runtime::RemoteObject> InjectedScript::wrapTable(
    v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const {
  v8::HandleScope handles(m_context->isolate());
  v8::Local<v8::Context> context = m_context->context();
  V8FunctionCall function(m_context->inspector(), context, v8Value(),
                          "wrapTable");
  function.appendArgument(table);
  if (columns.IsEmpty())
    function.appendArgument(false);
  else
    function.appendArgument(columns);
  bool hadException = false;
  v8::Local<v8::Value> r = function.call(hadException);
  if (hadException || r.IsEmpty()) return nullptr;
  protocol::ErrorString errorString;
  std::unique_ptr<protocol::Value> protocolValue =
      toProtocolValue(&errorString, context, r);
  if (!protocolValue) return nullptr;
  protocol::ErrorSupport errors;
  return protocol::Runtime::RemoteObject::parse(protocolValue.get(), &errors);
}

bool InjectedScript::findObject(ErrorString* errorString,
                                const RemoteObjectId& objectId,
                                v8::Local<v8::Value>* outObject) const {
  *outObject = m_native->objectForId(objectId.id());
  if (outObject->IsEmpty())
    *errorString = "Could not find object with given id";
  return !outObject->IsEmpty();
}

String16 InjectedScript::objectGroupName(const RemoteObjectId& objectId) const {
  return m_native->groupName(objectId.id());
}

void InjectedScript::releaseObjectGroup(const String16& objectGroup) {
  m_native->releaseObjectGroup(objectGroup);
  if (objectGroup == "console") m_lastEvaluationResult.Reset();
}

void InjectedScript::setCustomObjectFormatterEnabled(bool enabled) {
  v8::HandleScope handles(m_context->isolate());
  V8FunctionCall function(m_context->inspector(), m_context->context(),
                          v8Value(), "setCustomObjectFormatterEnabled");
  function.appendArgument(enabled);
  bool hadException = false;
  function.call(hadException);
  DCHECK(!hadException);
}

v8::Local<v8::Value> InjectedScript::v8Value() const {
  return m_value.Get(m_context->isolate());
}

v8::Local<v8::Value> InjectedScript::lastEvaluationResult() const {
  if (m_lastEvaluationResult.IsEmpty())
    return v8::Undefined(m_context->isolate());
  return m_lastEvaluationResult.Get(m_context->isolate());
}

v8::MaybeLocal<v8::Value> InjectedScript::resolveCallArgument(
    ErrorString* errorString, protocol::Runtime::CallArgument* callArgument) {
  if (callArgument->hasObjectId()) {
    std::unique_ptr<RemoteObjectId> remoteObjectId =
        RemoteObjectId::parse(errorString, callArgument->getObjectId(""));
    if (!remoteObjectId) return v8::MaybeLocal<v8::Value>();
    if (remoteObjectId->contextId() != m_context->contextId()) {
      *errorString =
          "Argument should belong to the same JavaScript world as target "
          "object";
      return v8::MaybeLocal<v8::Value>();
    }
    v8::Local<v8::Value> object;
    if (!findObject(errorString, *remoteObjectId, &object))
      return v8::MaybeLocal<v8::Value>();
    return object;
  }
  if (callArgument->hasValue() || callArgument->hasUnserializableValue()) {
    String16 value =
        callArgument->hasValue()
            ? callArgument->getValue(nullptr)->toJSONString()
            : "Number(\"" + callArgument->getUnserializableValue("") + "\")";
    v8::Local<v8::Value> object;
    if (!m_context->inspector()
             ->compileAndRunInternalScript(
                 m_context->context(), toV8String(m_context->isolate(), value))
             .ToLocal(&object)) {
      *errorString = "Couldn't parse value object in call argument";
      return v8::MaybeLocal<v8::Value>();
    }
    return object;
  }
  return v8::Undefined(m_context->isolate());
}

std::unique_ptr<protocol::Runtime::ExceptionDetails>
InjectedScript::createExceptionDetails(ErrorString* errorString,
                                       const v8::TryCatch& tryCatch,
                                       const String16& objectGroup,
                                       bool generatePreview) {
  if (!tryCatch.HasCaught()) return nullptr;
  v8::Local<v8::Message> message = tryCatch.Message();
  v8::Local<v8::Value> exception = tryCatch.Exception();
  String16 messageText =
      message.IsEmpty() ? String16() : toProtocolString(message->Get());
  std::unique_ptr<protocol::Runtime::ExceptionDetails> exceptionDetails =
      protocol::Runtime::ExceptionDetails::create()
          .setExceptionId(m_context->inspector()->nextExceptionId())
          .setText(exception.IsEmpty() ? messageText : String16("Uncaught"))
          .setLineNumber(
              message.IsEmpty()
                  ? 0
                  : message->GetLineNumber(m_context->context()).FromMaybe(1) -
                        1)
          .setColumnNumber(
              message.IsEmpty()
                  ? 0
                  : message->GetStartColumn(m_context->context()).FromMaybe(0))
          .build();
  if (!message.IsEmpty()) {
    exceptionDetails->setScriptId(String16::fromInteger(
        static_cast<int>(message->GetScriptOrigin().ScriptID()->Value())));
    v8::Local<v8::StackTrace> stackTrace = message->GetStackTrace();
    if (!stackTrace.IsEmpty() && stackTrace->GetFrameCount() > 0)
      exceptionDetails->setStackTrace(m_context->inspector()
                                          ->debugger()
                                          ->createStackTrace(stackTrace)
                                          ->buildInspectorObjectImpl());
  }
  if (!exception.IsEmpty()) {
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapped = wrapObject(
        errorString, exception, objectGroup, false /* forceValueType */,
        generatePreview && !exception->IsNativeError());
    if (!wrapped) return nullptr;
    exceptionDetails->setException(std::move(wrapped));
  }
  return exceptionDetails;
}

void InjectedScript::wrapEvaluateResult(
    ErrorString* errorString, v8::MaybeLocal<v8::Value> maybeResultValue,
    const v8::TryCatch& tryCatch, const String16& objectGroup,
    bool returnByValue, bool generatePreview,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  v8::Local<v8::Value> resultValue;
  if (!tryCatch.HasCaught()) {
    if (hasInternalError(errorString, !maybeResultValue.ToLocal(&resultValue)))
      return;
    std::unique_ptr<RemoteObject> remoteObject = wrapObject(
        errorString, resultValue, objectGroup, returnByValue, generatePreview);
    if (!remoteObject) return;
    if (objectGroup == "console")
      m_lastEvaluationResult.Reset(m_context->isolate(), resultValue);
    *result = std::move(remoteObject);
  } else {
    v8::Local<v8::Value> exception = tryCatch.Exception();
    std::unique_ptr<RemoteObject> remoteObject =
        wrapObject(errorString, exception, objectGroup, false,
                   generatePreview && !exception->IsNativeError());
    if (!remoteObject) return;
    // We send exception in result for compatibility reasons, even though it's
    // accessible through exceptionDetails.exception.
    *result = std::move(remoteObject);
    *exceptionDetails = createExceptionDetails(errorString, tryCatch,
                                               objectGroup, generatePreview);
  }
}

v8::Local<v8::Object> InjectedScript::commandLineAPI() {
  if (m_commandLineAPI.IsEmpty())
    m_commandLineAPI.Reset(m_context->isolate(),
                           V8Console::createCommandLineAPI(m_context));
  return m_commandLineAPI.Get(m_context->isolate());
}

InjectedScript::Scope::Scope(ErrorString* errorString,
                             V8InspectorImpl* inspector, int contextGroupId)
    : m_errorString(errorString),
      m_inspector(inspector),
      m_contextGroupId(contextGroupId),
      m_injectedScript(nullptr),
      m_handleScope(inspector->isolate()),
      m_tryCatch(inspector->isolate()),
      m_ignoreExceptionsAndMuteConsole(false),
      m_previousPauseOnExceptionsState(V8Debugger::DontPauseOnExceptions),
      m_userGesture(false) {}

bool InjectedScript::Scope::initialize() {
  cleanup();
  // TODO(dgozman): what if we reattach to the same context group during
  // evaluate? Introduce a session id?
  V8InspectorSessionImpl* session =
      m_inspector->sessionForContextGroup(m_contextGroupId);
  if (!session) {
    *m_errorString = "Internal error";
    return false;
  }
  findInjectedScript(session);
  if (!m_injectedScript) return false;
  m_context = m_injectedScript->context()->context();
  m_context->Enter();
  return true;
}

bool InjectedScript::Scope::installCommandLineAPI() {
  DCHECK(m_injectedScript && !m_context.IsEmpty() &&
         !m_commandLineAPIScope.get());
  m_commandLineAPIScope.reset(new V8Console::CommandLineAPIScope(
      m_context, m_injectedScript->commandLineAPI(), m_context->Global()));
  return true;
}

void InjectedScript::Scope::ignoreExceptionsAndMuteConsole() {
  DCHECK(!m_ignoreExceptionsAndMuteConsole);
  m_ignoreExceptionsAndMuteConsole = true;
  m_inspector->client()->muteMetrics(m_contextGroupId);
  m_inspector->muteExceptions(m_contextGroupId);
  m_previousPauseOnExceptionsState =
      setPauseOnExceptionsState(V8Debugger::DontPauseOnExceptions);
}

V8Debugger::PauseOnExceptionsState
InjectedScript::Scope::setPauseOnExceptionsState(
    V8Debugger::PauseOnExceptionsState newState) {
  if (!m_inspector->debugger()->enabled()) return newState;
  V8Debugger::PauseOnExceptionsState presentState =
      m_inspector->debugger()->getPauseOnExceptionsState();
  if (presentState != newState)
    m_inspector->debugger()->setPauseOnExceptionsState(newState);
  return presentState;
}

void InjectedScript::Scope::pretendUserGesture() {
  DCHECK(!m_userGesture);
  m_userGesture = true;
  m_inspector->client()->beginUserGesture();
}

void InjectedScript::Scope::cleanup() {
  m_commandLineAPIScope.reset();
  if (!m_context.IsEmpty()) {
    m_context->Exit();
    m_context.Clear();
  }
}

InjectedScript::Scope::~Scope() {
  if (m_ignoreExceptionsAndMuteConsole) {
    setPauseOnExceptionsState(m_previousPauseOnExceptionsState);
    m_inspector->client()->unmuteMetrics(m_contextGroupId);
    m_inspector->unmuteExceptions(m_contextGroupId);
  }
  if (m_userGesture) m_inspector->client()->endUserGesture();
  cleanup();
}

InjectedScript::ContextScope::ContextScope(ErrorString* errorString,
                                           V8InspectorImpl* inspector,
                                           int contextGroupId,
                                           int executionContextId)
    : InjectedScript::Scope(errorString, inspector, contextGroupId),
      m_executionContextId(executionContextId) {}

InjectedScript::ContextScope::~ContextScope() {}

void InjectedScript::ContextScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  m_injectedScript =
      session->findInjectedScript(m_errorString, m_executionContextId);
}

InjectedScript::ObjectScope::ObjectScope(ErrorString* errorString,
                                         V8InspectorImpl* inspector,
                                         int contextGroupId,
                                         const String16& remoteObjectId)
    : InjectedScript::Scope(errorString, inspector, contextGroupId),
      m_remoteObjectId(remoteObjectId) {}

InjectedScript::ObjectScope::~ObjectScope() {}

void InjectedScript::ObjectScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  std::unique_ptr<RemoteObjectId> remoteId =
      RemoteObjectId::parse(m_errorString, m_remoteObjectId);
  if (!remoteId) return;
  InjectedScript* injectedScript =
      session->findInjectedScript(m_errorString, remoteId.get());
  if (!injectedScript) return;
  m_objectGroupName = injectedScript->objectGroupName(*remoteId);
  if (!injectedScript->findObject(m_errorString, *remoteId, &m_object)) return;
  m_injectedScript = injectedScript;
}

InjectedScript::CallFrameScope::CallFrameScope(ErrorString* errorString,
                                               V8InspectorImpl* inspector,
                                               int contextGroupId,
                                               const String16& remoteObjectId)
    : InjectedScript::Scope(errorString, inspector, contextGroupId),
      m_remoteCallFrameId(remoteObjectId) {}

InjectedScript::CallFrameScope::~CallFrameScope() {}

void InjectedScript::CallFrameScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  std::unique_ptr<RemoteCallFrameId> remoteId =
      RemoteCallFrameId::parse(m_errorString, m_remoteCallFrameId);
  if (!remoteId) return;
  m_frameOrdinal = static_cast<size_t>(remoteId->frameOrdinal());
  m_injectedScript = session->findInjectedScript(m_errorString, remoteId.get());
}

}  // namespace v8_inspector
