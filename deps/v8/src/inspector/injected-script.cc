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
#include "src/inspector/v8-value-utils.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace {
static const char privateKeyName[] = "v8-inspector#injectedScript";
static const char kGlobalHandleLabel[] = "DevTools console";
static bool isResolvableNumberLike(String16 query) {
  return query == "Infinity" || query == "-Infinity" || query == "NaN";
}
}  // namespace

using protocol::Array;
using protocol::Runtime::PropertyDescriptor;
using protocol::Runtime::InternalPropertyDescriptor;
using protocol::Runtime::RemoteObject;
using protocol::Maybe;

class InjectedScript::ProtocolPromiseHandler {
 public:
  static bool add(V8InspectorSessionImpl* session,
                  v8::Local<v8::Context> context, v8::Local<v8::Value> value,
                  int executionContextId, const String16& objectGroup,
                  bool returnByValue, bool generatePreview,
                  EvaluateCallback* callback) {
    v8::Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) {
      callback->sendFailure(Response::InternalError());
      return false;
    }
    if (!resolver->Resolve(context, value).FromMaybe(false)) {
      callback->sendFailure(Response::InternalError());
      return false;
    }

    v8::Local<v8::Promise> promise = resolver->GetPromise();
    V8InspectorImpl* inspector = session->inspector();
    ProtocolPromiseHandler* handler =
        new ProtocolPromiseHandler(session, executionContextId, objectGroup,
                                   returnByValue, generatePreview, callback);
    v8::Local<v8::Value> wrapper = handler->m_wrapper.Get(inspector->isolate());
    v8::Local<v8::Function> thenCallbackFunction =
        v8::Function::New(context, thenCallback, wrapper, 0,
                          v8::ConstructorBehavior::kThrow)
            .ToLocalChecked();
    if (promise->Then(context, thenCallbackFunction).IsEmpty()) {
      callback->sendFailure(Response::InternalError());
      return false;
    }
    v8::Local<v8::Function> catchCallbackFunction =
        v8::Function::New(context, catchCallback, wrapper, 0,
                          v8::ConstructorBehavior::kThrow)
            .ToLocalChecked();
    if (promise->Catch(context, catchCallbackFunction).IsEmpty()) {
      callback->sendFailure(Response::InternalError());
      return false;
    }
    return true;
  }

 private:
  static void thenCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
    ProtocolPromiseHandler* handler = static_cast<ProtocolPromiseHandler*>(
        info.Data().As<v8::External>()->Value());
    DCHECK(handler);
    v8::Local<v8::Value> value =
        info.Length() > 0
            ? info[0]
            : v8::Local<v8::Value>::Cast(v8::Undefined(info.GetIsolate()));
    handler->thenCallback(value);
    delete handler;
  }

  static void catchCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
    ProtocolPromiseHandler* handler = static_cast<ProtocolPromiseHandler*>(
        info.Data().As<v8::External>()->Value());
    DCHECK(handler);
    v8::Local<v8::Value> value =
        info.Length() > 0
            ? info[0]
            : v8::Local<v8::Value>::Cast(v8::Undefined(info.GetIsolate()));
    handler->catchCallback(value);
    delete handler;
  }

  ProtocolPromiseHandler(V8InspectorSessionImpl* session,
                         int executionContextId, const String16& objectGroup,
                         bool returnByValue, bool generatePreview,
                         EvaluateCallback* callback)
      : m_inspector(session->inspector()),
        m_sessionId(session->sessionId()),
        m_contextGroupId(session->contextGroupId()),
        m_executionContextId(executionContextId),
        m_objectGroup(objectGroup),
        m_returnByValue(returnByValue),
        m_generatePreview(generatePreview),
        m_callback(std::move(callback)),
        m_wrapper(m_inspector->isolate(),
                  v8::External::New(m_inspector->isolate(), this)) {
    m_wrapper.SetWeak(this, cleanup, v8::WeakCallbackType::kParameter);
  }

  static void cleanup(
      const v8::WeakCallbackInfo<ProtocolPromiseHandler>& data) {
    if (!data.GetParameter()->m_wrapper.IsEmpty()) {
      data.GetParameter()->m_wrapper.Reset();
      data.SetSecondPassCallback(cleanup);
    } else {
      data.GetParameter()->sendPromiseCollected();
      delete data.GetParameter();
    }
  }

  void thenCallback(v8::Local<v8::Value> result) {
    V8InspectorSessionImpl* session =
        m_inspector->sessionById(m_contextGroupId, m_sessionId);
    if (!session) return;
    InjectedScript::ContextScope scope(session, m_executionContextId);
    Response response = scope.initialize();
    if (!response.isSuccess()) return;
    if (m_objectGroup == "console") {
      scope.injectedScript()->setLastEvaluationResult(result);
    }
    std::unique_ptr<EvaluateCallback> callback =
        scope.injectedScript()->takeEvaluateCallback(m_callback);
    if (!callback) return;
    std::unique_ptr<protocol::Runtime::RemoteObject> wrappedValue;
    response = scope.injectedScript()->wrapObject(
        result, m_objectGroup, m_returnByValue, m_generatePreview,
        &wrappedValue);
    if (!response.isSuccess()) {
      callback->sendFailure(response);
      return;
    }
    callback->sendSuccess(std::move(wrappedValue),
                          Maybe<protocol::Runtime::ExceptionDetails>());
  }

  void catchCallback(v8::Local<v8::Value> result) {
    V8InspectorSessionImpl* session =
        m_inspector->sessionById(m_contextGroupId, m_sessionId);
    if (!session) return;
    InjectedScript::ContextScope scope(session, m_executionContextId);
    Response response = scope.initialize();
    if (!response.isSuccess()) return;
    std::unique_ptr<EvaluateCallback> callback =
        scope.injectedScript()->takeEvaluateCallback(m_callback);
    if (!callback) return;
    std::unique_ptr<protocol::Runtime::RemoteObject> wrappedValue;
    response = scope.injectedScript()->wrapObject(
        result, m_objectGroup, m_returnByValue, m_generatePreview,
        &wrappedValue);
    if (!response.isSuccess()) {
      callback->sendFailure(response);
      return;
    }
    String16 message;
    std::unique_ptr<V8StackTraceImpl> stack;
    v8::Isolate* isolate = session->inspector()->isolate();
    if (result->IsNativeError()) {
      message = " " + toProtocolString(
                          isolate,
                          result->ToDetailString(isolate->GetCurrentContext())
                              .ToLocalChecked());
      v8::Local<v8::StackTrace> stackTrace = v8::debug::GetDetailedStackTrace(
          isolate, v8::Local<v8::Object>::Cast(result));
      if (!stackTrace.IsEmpty()) {
        stack = m_inspector->debugger()->createStackTrace(stackTrace);
      }
    }
    if (!stack) {
      stack = m_inspector->debugger()->captureStackTrace(true);
    }
    std::unique_ptr<protocol::Runtime::ExceptionDetails> exceptionDetails =
        protocol::Runtime::ExceptionDetails::create()
            .setExceptionId(m_inspector->nextExceptionId())
            .setText("Uncaught (in promise)" + message)
            .setLineNumber(stack && !stack->isEmpty() ? stack->topLineNumber()
                                                      : 0)
            .setColumnNumber(
                stack && !stack->isEmpty() ? stack->topColumnNumber() : 0)
            .setException(wrappedValue->clone())
            .build();
    if (stack)
      exceptionDetails->setStackTrace(
          stack->buildInspectorObjectImpl(m_inspector->debugger()));
    if (stack && !stack->isEmpty())
      exceptionDetails->setScriptId(toString16(stack->topScriptId()));
    callback->sendSuccess(std::move(wrappedValue), std::move(exceptionDetails));
  }

  void sendPromiseCollected() {
    V8InspectorSessionImpl* session =
        m_inspector->sessionById(m_contextGroupId, m_sessionId);
    if (!session) return;
    InjectedScript::ContextScope scope(session, m_executionContextId);
    Response response = scope.initialize();
    if (!response.isSuccess()) return;
    std::unique_ptr<EvaluateCallback> callback =
        scope.injectedScript()->takeEvaluateCallback(m_callback);
    if (!callback) return;
    callback->sendFailure(Response::Error("Promise was collected"));
  }

  V8InspectorImpl* m_inspector;
  int m_sessionId;
  int m_contextGroupId;
  int m_executionContextId;
  String16 m_objectGroup;
  bool m_returnByValue;
  bool m_generatePreview;
  EvaluateCallback* m_callback;
  v8::Global<v8::External> m_wrapper;
};

std::unique_ptr<InjectedScript> InjectedScript::create(
    InspectedContext* inspectedContext, int sessionId) {
  v8::Isolate* isolate = inspectedContext->isolate();
  v8::HandleScope handles(isolate);
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::Context> context = inspectedContext->context();
  v8::debug::PostponeInterruptsScope postponeInterrupts(isolate);
  v8::Context::Scope scope(context);
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

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
  StringView injectedScriptSource(
      reinterpret_cast<const uint8_t*>(InjectedScriptSource_js),
      sizeof(InjectedScriptSource_js));
  v8::Local<v8::Value> value;
  if (!inspectedContext->inspector()
           ->compileAndRunInternalScript(
               context, toV8String(isolate, injectedScriptSource))
           .ToLocal(&value)) {
    return nullptr;
  }
  DCHECK(value->IsFunction());
  v8::Local<v8::Object> scriptHostWrapper =
      V8InjectedScriptHost::create(context, inspectedContext->inspector());
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  v8::Local<v8::Object> windowGlobal = context->Global();
  v8::Local<v8::Value> info[] = {
      scriptHostWrapper, windowGlobal,
      v8::Number::New(isolate, inspectedContext->contextId())};

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

  std::unique_ptr<InjectedScript> injectedScript(new InjectedScript(
      inspectedContext, injectedScriptValue.As<v8::Object>(), sessionId));
  v8::Local<v8::Private> privateKey = v8::Private::ForApi(
      isolate, v8::String::NewFromUtf8(isolate, privateKeyName,
                                       v8::NewStringType::kInternalized)
                   .ToLocalChecked());
  scriptHostWrapper->SetPrivate(
      context, privateKey, v8::External::New(isolate, injectedScript.get()));
  return injectedScript;
}

InjectedScript::InjectedScript(InspectedContext* context,
                               v8::Local<v8::Object> object, int sessionId)
    : m_context(context),
      m_value(context->isolate(), object),
      m_sessionId(sessionId) {}

InjectedScript::~InjectedScript() { discardEvaluateCallbacks(); }

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
  unbindObject(boundId);
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

void InjectedScript::addPromiseCallback(
    V8InspectorSessionImpl* session, v8::MaybeLocal<v8::Value> value,
    const String16& objectGroup, bool returnByValue, bool generatePreview,
    std::unique_ptr<EvaluateCallback> callback) {
  if (value.IsEmpty()) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  v8::MicrotasksScope microtasksScope(m_context->isolate(),
                                      v8::MicrotasksScope::kRunMicrotasks);
  if (ProtocolPromiseHandler::add(
          session, m_context->context(), value.ToLocalChecked(),
          m_context->contextId(), objectGroup, returnByValue, generatePreview,
          callback.get())) {
    m_evaluateCallbacks.insert(callback.release());
  }
}

void InjectedScript::discardEvaluateCallbacks() {
  for (auto& callback : m_evaluateCallbacks) {
    callback->sendFailure(Response::Error("Execution context was destroyed."));
    delete callback;
  }
  m_evaluateCallbacks.clear();
}

std::unique_ptr<EvaluateCallback> InjectedScript::takeEvaluateCallback(
    EvaluateCallback* callback) {
  auto it = m_evaluateCallbacks.find(callback);
  if (it == m_evaluateCallbacks.end()) return nullptr;
  std::unique_ptr<EvaluateCallback> value(*it);
  m_evaluateCallbacks.erase(it);
  return value;
}

Response InjectedScript::findObject(const RemoteObjectId& objectId,
                                    v8::Local<v8::Value>* outObject) const {
  auto it = m_idToWrappedObject.find(objectId.id());
  if (it == m_idToWrappedObject.end())
    return Response::Error("Could not find object with given id");
  *outObject = it->second.Get(m_context->isolate());
  return Response::OK();
}

String16 InjectedScript::objectGroupName(const RemoteObjectId& objectId) const {
  if (objectId.id() <= 0) return String16();
  auto it = m_idToObjectGroupName.find(objectId.id());
  return it != m_idToObjectGroupName.end() ? it->second : String16();
}

void InjectedScript::releaseObjectGroup(const String16& objectGroup) {
  if (objectGroup == "console") m_lastEvaluationResult.Reset();
  if (objectGroup.isEmpty()) return;
  auto it = m_nameToObjectGroup.find(objectGroup);
  if (it == m_nameToObjectGroup.end()) return;
  for (int id : it->second) unbindObject(id);
  m_nameToObjectGroup.erase(it);
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

void InjectedScript::setLastEvaluationResult(v8::Local<v8::Value> result) {
  m_lastEvaluationResult.Reset(m_context->isolate(), result);
  m_lastEvaluationResult.AnnotateStrongRetainer(kGlobalHandleLabel);
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
    String16 value;
    if (callArgument->hasValue()) {
      value = "(" + callArgument->getValue(nullptr)->serialize() + ")";
    } else {
      String16 unserializableValue = callArgument->getUnserializableValue("");
      // Protect against potential identifier resolution for NaN and Infinity.
      if (isResolvableNumberLike(unserializableValue))
        value = "Number(\"" + unserializableValue + "\")";
      else
        value = unserializableValue;
    }
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
      message.IsEmpty()
          ? String16()
          : toProtocolString(m_context->isolate(), message->Get());
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
      exceptionDetails->setStackTrace(
          m_context->inspector()
              ->debugger()
              ->createStackTrace(stackTrace)
              ->buildInspectorObjectImpl(m_context->inspector()->debugger()));
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
    if (objectGroup == "console") {
      m_lastEvaluationResult.Reset(m_context->isolate(), resultValue);
      m_lastEvaluationResult.AnnotateStrongRetainer(kGlobalHandleLabel);
    }
  } else {
    if (tryCatch.HasTerminated() || !tryCatch.CanContinue()) {
      return Response::Error("Execution was terminated");
    }
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
            m_context->context(), m_sessionId));
    m_commandLineAPI.AnnotateStrongRetainer(kGlobalHandleLabel);
  }
  return m_commandLineAPI.Get(m_context->isolate());
}

InjectedScript::Scope::Scope(V8InspectorSessionImpl* session)
    : m_inspector(session->inspector()),
      m_injectedScript(nullptr),
      m_handleScope(m_inspector->isolate()),
      m_tryCatch(m_inspector->isolate()),
      m_ignoreExceptionsAndMuteConsole(false),
      m_previousPauseOnExceptionsState(v8::debug::NoBreakOnException),
      m_userGesture(false),
      m_allowEval(false),
      m_contextGroupId(session->contextGroupId()),
      m_sessionId(session->sessionId()) {}

Response InjectedScript::Scope::initialize() {
  cleanup();
  V8InspectorSessionImpl* session =
      m_inspector->sessionById(m_contextGroupId, m_sessionId);
  if (!session) return Response::InternalError();
  Response response = findInjectedScript(session);
  if (!response.isSuccess()) return response;
  m_context = m_injectedScript->context()->context();
  m_context->Enter();
  if (m_allowEval) m_context->AllowCodeGenerationFromStrings(true);
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

void InjectedScript::Scope::allowCodeGenerationFromStrings() {
  DCHECK(!m_allowEval);
  if (m_context->IsCodeGenerationFromStringsAllowed()) return;
  m_allowEval = true;
  m_context->AllowCodeGenerationFromStrings(true);
}

void InjectedScript::Scope::cleanup() {
  m_commandLineAPIScope.reset();
  if (!m_context.IsEmpty()) {
    if (m_allowEval) m_context->AllowCodeGenerationFromStrings(false);
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

InjectedScript::ContextScope::ContextScope(V8InspectorSessionImpl* session,
                                           int executionContextId)
    : InjectedScript::Scope(session),
      m_executionContextId(executionContextId) {}

InjectedScript::ContextScope::~ContextScope() = default;

Response InjectedScript::ContextScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  return session->findInjectedScript(m_executionContextId, m_injectedScript);
}

InjectedScript::ObjectScope::ObjectScope(V8InspectorSessionImpl* session,
                                         const String16& remoteObjectId)
    : InjectedScript::Scope(session), m_remoteObjectId(remoteObjectId) {}

InjectedScript::ObjectScope::~ObjectScope() = default;

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

InjectedScript::CallFrameScope::CallFrameScope(V8InspectorSessionImpl* session,
                                               const String16& remoteObjectId)
    : InjectedScript::Scope(session), m_remoteCallFrameId(remoteObjectId) {}

InjectedScript::CallFrameScope::~CallFrameScope() = default;

Response InjectedScript::CallFrameScope::findInjectedScript(
    V8InspectorSessionImpl* session) {
  std::unique_ptr<RemoteCallFrameId> remoteId;
  Response response = RemoteCallFrameId::parse(m_remoteCallFrameId, &remoteId);
  if (!response.isSuccess()) return response;
  m_frameOrdinal = static_cast<size_t>(remoteId->frameOrdinal());
  return session->findInjectedScript(remoteId.get(), m_injectedScript);
}

InjectedScript* InjectedScript::fromInjectedScriptHost(
    v8::Isolate* isolate, v8::Local<v8::Object> injectedScriptObject) {
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Private> privateKey = v8::Private::ForApi(
      isolate, v8::String::NewFromUtf8(isolate, privateKeyName,
                                       v8::NewStringType::kInternalized)
                   .ToLocalChecked());
  v8::Local<v8::Value> value =
      injectedScriptObject->GetPrivate(context, privateKey).ToLocalChecked();
  DCHECK(value->IsExternal());
  v8::Local<v8::External> external = value.As<v8::External>();
  return static_cast<InjectedScript*>(external->Value());
}

int InjectedScript::bindObject(v8::Local<v8::Value> value,
                               const String16& groupName) {
  if (m_lastBoundObjectId <= 0) m_lastBoundObjectId = 1;
  int id = m_lastBoundObjectId++;
  m_idToWrappedObject[id].Reset(m_context->isolate(), value);
  m_idToWrappedObject[id].AnnotateStrongRetainer(kGlobalHandleLabel);

  if (!groupName.isEmpty() && id > 0) {
    m_idToObjectGroupName[id] = groupName;
    m_nameToObjectGroup[groupName].push_back(id);
  }
  return id;
}

void InjectedScript::unbindObject(int id) {
  m_idToWrappedObject.erase(id);
  m_idToObjectGroupName.erase(id);
}

}  // namespace v8_inspector
