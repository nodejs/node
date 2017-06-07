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

  int contextGroupId = inspectedContext->contextGroupId();
  int contextId = inspectedContext->contextId();
  V8InspectorImpl* inspector = inspectedContext->inspector();
  v8::Local<v8::Value> injectedScriptValue;
  if (!function->Call(context, windowGlobal, arraysize(info), info)
           .ToLocal(&injectedScriptValue))
    return nullptr;
  if (inspector->getContext(contextGroupId, contextId) != inspectedContext)
    return nullptr;
  if (!injectedScriptValue->IsObject()) return nullptr;
  return std::unique_ptr<InjectedScript>(
      new InjectedScript(inspectedContext, injectedScriptValue.As<v8::Object>(),
                         std::move(injectedScriptNative)));
}

InjectedScript::InjectedScript(
    InspectedContext* context, v8::Local<v8::Object> object,
    std::unique_ptr<InjectedScriptNative> injectedScriptNative)
    : m_context(context),
      m_value(context->isolate(), object),
      m_native(std::move(injectedScriptNative)) {}

InjectedScript::~InjectedScript() {}

Response InjectedScript::getProperties(
    v8::Local<v8::Object> object, const String16& groupName, bool ownProperties,
    bool accessorPropertiesOnly, bool generatePreview,
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
    Response response = createExceptionDetails(
        tryCatch, groupName, generatePreview, exceptionDetails);
    if (!response.isSuccess()) return response;
    // FIXME: make properties optional
    *properties = Array<PropertyDescriptor>::create();
    return Response::OK();
  }
  if (resultValue.IsEmpty()) return Response::InternalError();
  std::unique_ptr<protocol::Value> protocolValue;
  Response response = toProtocolValue(context, resultValue, &protocolValue);
  if (!response.isSuccess()) return response;
  protocol::ErrorSupport errors;
  std::unique_ptr<Array<PropertyDescriptor>> result =
      Array<PropertyDescriptor>::fromValue(protocolValue.get(), &errors);
  if (errors.hasErrors()) return Response::Error(errors.errors());
  *properties = std::move(result);
  return Response::OK();
}

void InjectedScript::releaseObject(const String16& objectId) {
  std::unique_ptr<protocol::Value> parsedObjectId =
      protocol::StringUtil::parseJSON(objectId);
  if (!parsedObjectId) return;
  protocol::DictionaryValue* object =
      protocol::DictionaryValue::cast(parsedObjectId.get());
  if (!object) return;
  int boundId = 0;
  if (!object->getInteger("id", &boundId)) return;
  m_native->unbind(boundId);
}

Response InjectedScript::wrapObject(
    v8::Local<v8::Value> value, const String16& groupName, bool forceValueType,
    bool generatePreview,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result) const {
  v8::HandleScope handles(m_context->isolate());
  v8::Local<v8::Value> wrappedObject;
  v8::Local<v8::Context> context = m_context->context();
  Response response = wrapValue(value, groupName, forceValueType,
                                generatePreview, &wrappedObject);
  if (!response.isSuccess()) return response;
  protocol::ErrorSupport errors;
  std::unique_ptr<protocol::Value> protocolValue;
  response = toProtocolValue(context, wrappedObject, &protocolValue);
  if (!response.isSuccess()) return response;

  *result =
      protocol::Runtime::RemoteObject::fromValue(protocolValue.get(), &errors);
  if (!result->get()) return Response::Error(errors.errors());
  return Response::OK();
}

Response InjectedScript::wrapObjectProperty(v8::Local<v8::Object> object,
                                            v8::Local<v8::Name> key,
                                            const String16& groupName,
                                            bool forceValueType,
                                            bool generatePreview) const {
  v8::Local<v8::Value> property;
  v8::Local<v8::Context> context = m_context->context();
  if (!object->Get(context, key).ToLocal(&property))
    return Response::InternalError();
  v8::Local<v8::Value> wrappedProperty;
  Response response = wrapValue(property, groupName, forceValueType,
                                generatePreview, &wrappedProperty);
  if (!response.isSuccess()) return response;
  v8::Maybe<bool> success =
      createDataProperty(context, object, key, wrappedProperty);
  if (success.IsNothing() || !success.FromJust())
    return Response::InternalError();
  return Response::OK();
}

Response InjectedScript::wrapPropertyInArray(v8::Local<v8::Array> array,
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
  return hadException ? Response::InternalError() : Response::OK();
}

Response InjectedScript::wrapValue(v8::Local<v8::Value> value,
                                   const String16& groupName,
                                   bool forceValueType, bool generatePreview,
                                   v8::Local<v8::Value>* result) const {
  V8FunctionCall function(m_context->inspector(), m_context->context(),
                          v8Value(), "wrapObject");
  function.appendArgument(value);
  function.appendArgument(groupName);
  function.appendArgument(forceValueType);
  function.appendArgument(generatePreview);
  bool hadException = false;
  *result = function.call(hadException);
  if (hadException || result->IsEmpty()) return Response::InternalError();
  return Response::OK();
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
  std::unique_ptr<protocol::Value> protocolValue;
  Response response = toProtocolValue(context, r, &protocolValue);
  if (!response.isSuccess()) return nullptr;
  protocol::ErrorSupport errors;
  return protocol::Runtime::RemoteObject::fromValue(protocolValue.get(),
                                                    &errors);
}

Response InjectedScript::findObject(const RemoteObjectId& objectId,
                                    v8::Local<v8::Value>* outObject) const {
  *outObject = m_native->objectForId(objectId.id());
  if (outObject->IsEmpty())
    return Response::Error("Could not find object with given id");
  return Response::OK();
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

Response InjectedScript::resolveCallArgument(
    protocol::Runtime::CallArgument* callArgument,
    v8::Local<v8::Value>* result) {
  if (callArgument->hasObjectId()) {
    std::unique_ptr<RemoteObjectId> remoteObjectId;
    Response response =
        RemoteObjectId::parse(callArgument->getObjectId(""), &remoteObjectId);
    if (!response.isSuccess()) return response;
    if (remoteObjectId->contextId() != m_context->contextId())
      return Response::Error(
          "Argument should belong to the same JavaScript world as target "
          "object");
    return findObject(*remoteObjectId, result);
  }
  if (callArgument->hasValue() || callArgument->hasUnserializableValue()) {
    String16 value =
        callArgument->hasValue()
            ? callArgument->getValue(nullptr)->serialize()
            : "Number(\"" + callArgument->getUnserializableValue("") + "\")";
    if (!m_context->inspector()
             ->compileAndRunInternalScript(
                 m_context->context(), toV8String(m_context->isolate(), value))
             .ToLocal(result)) {
      return Response::Error("Couldn't parse value object in call argument");
    }
    return Response::OK();
  }
  *result = v8::Undefined(m_context->isolate());
  return Response::OK();
}

Response InjectedScript::createExceptionDetails(
    const v8::TryCatch& tryCatch, const String16& objectGroup,
    bool generatePreview, Maybe<protocol::Runtime::ExceptionDetails>* result) {
  if (!tryCatch.HasCaught()) return Response::InternalError();
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
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapped;
    Response response =
        wrapObject(exception, objectGroup, false /* forceValueType */,
                   generatePreview && !exception->IsNativeError(), &wrapped);
    if (!response.isSuccess()) return response;
    exceptionDetails->setException(std::move(wrapped));
  }
  *result = std::move(exceptionDetails);
  return Response::OK();
}

Response InjectedScript::wrapEvaluateResult(
    v8::MaybeLocal<v8::Value> maybeResultValue, const v8::TryCatch& tryCatch,
    const String16& objectGroup, bool returnByValue, bool generatePreview,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  v8::Local<v8::Value> resultValue;
  if (!tryCatch.HasCaught()) {
    if (!maybeResultValue.ToLocal(&resultValue))
      return Response::InternalError();
    Response response = wrapObject(resultValue, objectGroup, returnByValue,
                                   generatePreview, result);
    if (!response.isSuccess()) return response;
    if (objectGroup == "console")
      m_lastEvaluationResult.Reset(m_context->isolate(), resultValue);
  } else {
    v8::Local<v8::Value> exception = tryCatch.Exception();
    Response response =
        wrapObject(exception, objectGroup, false,
                   generatePreview && !exception->IsNativeError(), result);
    if (!response.isSuccess()) return response;
    // We send exception in result for compatibility reasons, even though it's
    // accessible through exceptionDetails.exception.
    response = createExceptionDetails(tryCatch, objectGroup, generatePreview,
                                      exceptionDetails);
    if (!response.isSuccess()) return response;
  }
  return Response::OK();
}

v8::Local<v8::Object> InjectedScript::commandLineAPI() {
  if (m_commandLineAPI.IsEmpty()) {
    m_commandLineAPI.Reset(
        m_context->isolate(),
        m_context->inspector()->console()->createCommandLineAPI(
            m_context->context()));
  }
  return m_commandLineAPI.Get(m_context->isolate());
}

InjectedScript::Scope::Scope(V8InspectorImpl* inspector, int contextGroupId)
    : m_inspector(inspector),
      m_contextGroupId(contextGroupId),
      m_injectedScript(nullptr),
      m_handleScope(inspector->isolate()),
      m_tryCatch(inspector->isolate()),
      m_ignoreExceptionsAndMuteConsole(false),
      m_previousPauseOnExceptionsState(v8::debug::NoBreakOnException),
      m_userGesture(false) {}

Response InjectedScript::Scope::initialize() {
  cleanup();
  // TODO(dgozman): what if we reattach to the same context group during
  // evaluate? Introduce a session id?
  V8InspectorSessionImpl* session =
      m_inspector->sessionForContextGroup(m_contextGroupId);
  if (!session) return Response::InternalError();
  Response response = findInjectedScript(session);
  if (!response.isSuccess()) return response;
  m_context = m_injectedScript->context()->context();
  m_context->Enter();
  return Response::OK();
}

void InjectedScript::Scope::installCommandLineAPI() {
  DCHECK(m_injectedScript && !m_context.IsEmpty() &&
         !m_commandLineAPIScope.get());
  m_commandLineAPIScope.reset(new V8Console::CommandLineAPIScope(
      m_context, m_injectedScript->commandLineAPI(), m_context->Global()));
}

void InjectedScript::Scope::ignoreExceptionsAndMuteConsole() {
  DCHECK(!m_ignoreExceptionsAndMuteConsole);
  m_ignoreExceptionsAndMuteConsole = true;
  m_inspector->client()->muteMetrics(m_contextGroupId);
  m_inspector->muteExceptions(m_contextGroupId);
  m_previousPauseOnExceptionsState =
      setPauseOnExceptionsState(v8::debug::NoBreakOnException);
}

v8::debug::ExceptionBreakState InjectedScript::Scope::setPauseOnExceptionsState(
    v8::debug::ExceptionBreakState newState) {
  if (!m_inspector->debugger()->enabled()) return newState;
  v8::debug::ExceptionBreakState presentState =
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

InjectedScript::ContextScope::ContextScope(V8InspectorImpl* inspector,
                                           int contextGroupId,
                                           int executionContextId)
    : InjectedScript::Scope(inspector, contextGroupId),
      m_executionContextId(executionContextId) {}

InjectedScript::ContextScope::~ContextScope() {}

Response InjectedScript::ContextScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  return session->findInjectedScript(m_executionContextId, m_injectedScript);
}

InjectedScript::ObjectScope::ObjectScope(V8InspectorImpl* inspector,
                                         int contextGroupId,
                                         const String16& remoteObjectId)
    : InjectedScript::Scope(inspector, contextGroupId),
      m_remoteObjectId(remoteObjectId) {}

InjectedScript::ObjectScope::~ObjectScope() {}

Response InjectedScript::ObjectScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  std::unique_ptr<RemoteObjectId> remoteId;
  Response response = RemoteObjectId::parse(m_remoteObjectId, &remoteId);
  if (!response.isSuccess()) return response;
  InjectedScript* injectedScript = nullptr;
  response = session->findInjectedScript(remoteId.get(), injectedScript);
  if (!response.isSuccess()) return response;
  m_objectGroupName = injectedScript->objectGroupName(*remoteId);
  response = injectedScript->findObject(*remoteId, &m_object);
  if (!response.isSuccess()) return response;
  m_injectedScript = injectedScript;
  return Response::OK();
}

InjectedScript::CallFrameScope::CallFrameScope(V8InspectorImpl* inspector,
                                               int contextGroupId,
                                               const String16& remoteObjectId)
    : InjectedScript::Scope(inspector, contextGroupId),
      m_remoteCallFrameId(remoteObjectId) {}

InjectedScript::CallFrameScope::~CallFrameScope() {}

Response InjectedScript::CallFrameScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  std::unique_ptr<RemoteCallFrameId> remoteId;
  Response response = RemoteCallFrameId::parse(m_remoteCallFrameId, &remoteId);
  if (!response.isSuccess()) return response;
  m_frameOrdinal = static_cast<size_t>(remoteId->frameOrdinal());
  return session->findInjectedScript(remoteId.get(), m_injectedScript);
}

}  // namespace v8_inspector
