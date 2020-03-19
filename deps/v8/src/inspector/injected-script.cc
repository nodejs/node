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

#include <cmath>
#include <unordered_set>

#include "../../third_party/inspector_protocol/crdtp/json.h"
#include "src/inspector/custom-preview.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/remote-object-id.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"
#include "src/inspector/v8-value-utils.h"
#include "src/inspector/value-mirror.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace {
const char kGlobalHandleLabel[] = "DevTools console";
bool isResolvableNumberLike(String16 query) {
  return query == "Infinity" || query == "-Infinity" || query == "NaN";
}
}  // namespace

using protocol::Array;
using protocol::Maybe;
using protocol::Runtime::InternalPropertyDescriptor;
using protocol::Runtime::PrivatePropertyDescriptor;
using protocol::Runtime::PropertyDescriptor;
using protocol::Runtime::RemoteObject;

class InjectedScript::ProtocolPromiseHandler {
 public:
  static bool add(V8InspectorSessionImpl* session,
                  v8::Local<v8::Context> context, v8::Local<v8::Value> value,
                  int executionContextId, const String16& objectGroup,
                  WrapMode wrapMode, bool replMode,
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

    v8::MaybeLocal<v8::Promise> originalPromise =
        value->IsPromise() ? v8::Local<v8::Promise>::Cast(value)
                           : v8::MaybeLocal<v8::Promise>();
    V8InspectorImpl* inspector = session->inspector();
    ProtocolPromiseHandler* handler = new ProtocolPromiseHandler(
        session, executionContextId, objectGroup, wrapMode, replMode, callback,
        originalPromise);
    v8::Local<v8::Value> wrapper = handler->m_wrapper.Get(inspector->isolate());
    v8::Local<v8::Function> thenCallbackFunction =
        v8::Function::New(context, thenCallback, wrapper, 0,
                          v8::ConstructorBehavior::kThrow)
            .ToLocalChecked();
    v8::Local<v8::Function> catchCallbackFunction =
        v8::Function::New(context, catchCallback, wrapper, 0,
                          v8::ConstructorBehavior::kThrow)
            .ToLocalChecked();
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    if (promise->Then(context, thenCallbackFunction, catchCallbackFunction)
            .IsEmpty()) {
      callback->sendFailure(Response::InternalError());
      return false;
    }
    return true;
  }

 private:
  static v8::Local<v8::String> GetDotReplResultString(v8::Isolate* isolate) {
    // TODO(szuend): Cache the string in a v8::Persistent handle.
    return v8::String::NewFromOneByte(
               isolate, reinterpret_cast<const uint8_t*>(".repl_result"))
        .ToLocalChecked();
  }

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
                         WrapMode wrapMode, bool replMode,
                         EvaluateCallback* callback,
                         v8::MaybeLocal<v8::Promise> maybeEvaluationResult)
      : m_inspector(session->inspector()),
        m_sessionId(session->sessionId()),
        m_contextGroupId(session->contextGroupId()),
        m_executionContextId(executionContextId),
        m_objectGroup(objectGroup),
        m_wrapMode(wrapMode),
        m_replMode(replMode),
        m_callback(std::move(callback)),
        m_wrapper(m_inspector->isolate(),
                  v8::External::New(m_inspector->isolate(), this)) {
    m_wrapper.SetWeak(this, cleanup, v8::WeakCallbackType::kParameter);
    v8::Local<v8::Promise> promise;
    if (maybeEvaluationResult.ToLocal(&promise)) {
      m_evaluationResult =
          v8::Global<v8::Promise>(m_inspector->isolate(), promise);
    }
  }

  static void cleanup(
      const v8::WeakCallbackInfo<ProtocolPromiseHandler>& data) {
    if (!data.GetParameter()->m_wrapper.IsEmpty()) {
      data.GetParameter()->m_wrapper.Reset();
      data.GetParameter()->m_evaluationResult.Reset();
      data.SetSecondPassCallback(cleanup);
    } else {
      data.GetParameter()->sendPromiseCollected();
      delete data.GetParameter();
    }
  }

  void thenCallback(v8::Local<v8::Value> value) {
    V8InspectorSessionImpl* session =
        m_inspector->sessionById(m_contextGroupId, m_sessionId);
    if (!session) return;
    InjectedScript::ContextScope scope(session, m_executionContextId);
    Response response = scope.initialize();
    if (!response.isSuccess()) return;

    std::unique_ptr<EvaluateCallback> callback =
        scope.injectedScript()->takeEvaluateCallback(m_callback);
    if (!callback) return;

    // In REPL mode the result is additionally wrapped in an object.
    // The evaluation result can be found at ".repl_result".
    v8::Local<v8::Value> result = value;
    if (m_replMode) {
      v8::Local<v8::Object> object;
      if (!result->ToObject(scope.context()).ToLocal(&object)) {
        callback->sendFailure(response);
        return;
      }

      v8::Local<v8::String> name =
          GetDotReplResultString(m_inspector->isolate());
      if (!object->Get(scope.context(), name).ToLocal(&result)) {
        callback->sendFailure(response);
        return;
      }
    }

    if (m_objectGroup == "console") {
      scope.injectedScript()->setLastEvaluationResult(result);
    }

    std::unique_ptr<protocol::Runtime::RemoteObject> wrappedValue;
    response = scope.injectedScript()->wrapObject(result, m_objectGroup,
                                                  m_wrapMode, &wrappedValue);
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
    response = scope.injectedScript()->wrapObject(result, m_objectGroup,
                                                  m_wrapMode, &wrappedValue);
    if (!response.isSuccess()) {
      callback->sendFailure(response);
      return;
    }
    v8::Isolate* isolate = session->inspector()->isolate();

    v8::MaybeLocal<v8::Message> maybeMessage =
        m_evaluationResult.IsEmpty()
            ? v8::MaybeLocal<v8::Message>()
            : v8::debug::GetMessageFromPromise(m_evaluationResult.Get(isolate));
    v8::Local<v8::Message> message;
    // In case a MessageObject was attached to the rejected promise, we
    // construct the exception details from the message object. Otherwise
    // we try to capture a fresh stack trace.
    if (maybeMessage.ToLocal(&message)) {
      v8::Local<v8::Value> exception = result;
      protocol::detail::PtrMaybe<protocol::Runtime::ExceptionDetails>
          exceptionDetails;
      response = scope.injectedScript()->createExceptionDetails(
          message, exception, m_objectGroup, &exceptionDetails);
      if (!response.isSuccess()) {
        callback->sendFailure(response);
        return;
      }

      callback->sendSuccess(std::move(wrappedValue),
                            std::move(exceptionDetails));
      return;
    }

    String16 messageString;
    std::unique_ptr<V8StackTraceImpl> stack;
    if (result->IsNativeError()) {
      messageString =
          " " +
          toProtocolString(isolate,
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

    // REPL mode implicitly handles the script like an async function.
    // Do not prepend the '(in promise)' prefix for these exceptions since that
    // would be confusing for the user. The stringified error is part of the
    // exception and does not need to be added in REPL mode, otherwise it would
    // be printed twice.
    String16 exceptionDetailsText =
        m_replMode ? "Uncaught" : "Uncaught (in promise)" + messageString;
    std::unique_ptr<protocol::Runtime::ExceptionDetails> exceptionDetails =
        protocol::Runtime::ExceptionDetails::create()
            .setExceptionId(m_inspector->nextExceptionId())
            .setText(exceptionDetailsText)
            .setLineNumber(stack && !stack->isEmpty() ? stack->topLineNumber()
                                                      : 0)
            .setColumnNumber(
                stack && !stack->isEmpty() ? stack->topColumnNumber() : 0)
            .build();
    response = scope.injectedScript()->addExceptionToDetails(
        result, exceptionDetails.get(), m_objectGroup);
    if (!response.isSuccess()) {
      callback->sendFailure(response);
      return;
    }
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
  WrapMode m_wrapMode;
  bool m_replMode;
  EvaluateCallback* m_callback;
  v8::Global<v8::External> m_wrapper;
  v8::Global<v8::Promise> m_evaluationResult;
};

InjectedScript::InjectedScript(InspectedContext* context, int sessionId)
    : m_context(context), m_sessionId(sessionId) {}

InjectedScript::~InjectedScript() { discardEvaluateCallbacks(); }

namespace {
class PropertyAccumulator : public ValueMirror::PropertyAccumulator {
 public:
  explicit PropertyAccumulator(std::vector<PropertyMirror>* mirrors)
      : m_mirrors(mirrors) {}
  bool Add(PropertyMirror mirror) override {
    m_mirrors->push_back(std::move(mirror));
    return true;
  }

 private:
  std::vector<PropertyMirror>* m_mirrors;
};
}  // anonymous namespace

Response InjectedScript::getProperties(
    v8::Local<v8::Object> object, const String16& groupName, bool ownProperties,
    bool accessorPropertiesOnly, WrapMode wrapMode,
    std::unique_ptr<Array<PropertyDescriptor>>* properties,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  v8::HandleScope handles(m_context->isolate());
  v8::Local<v8::Context> context = m_context->context();
  v8::Isolate* isolate = m_context->isolate();
  int sessionId = m_sessionId;
  v8::TryCatch tryCatch(isolate);

  *properties = std::make_unique<Array<PropertyDescriptor>>();
  std::vector<PropertyMirror> mirrors;
  PropertyAccumulator accumulator(&mirrors);
  if (!ValueMirror::getProperties(context, object, ownProperties,
                                  accessorPropertiesOnly, &accumulator)) {
    return createExceptionDetails(tryCatch, groupName, exceptionDetails);
  }
  for (const PropertyMirror& mirror : mirrors) {
    std::unique_ptr<PropertyDescriptor> descriptor =
        PropertyDescriptor::create()
            .setName(mirror.name)
            .setConfigurable(mirror.configurable)
            .setEnumerable(mirror.enumerable)
            .setIsOwn(mirror.isOwn)
            .build();
    Response response;
    std::unique_ptr<RemoteObject> remoteObject;
    if (mirror.value) {
      response = wrapObjectMirror(*mirror.value, groupName, wrapMode,
                                  v8::MaybeLocal<v8::Value>(),
                                  kMaxCustomPreviewDepth, &remoteObject);
      if (!response.isSuccess()) return response;
      descriptor->setValue(std::move(remoteObject));
      descriptor->setWritable(mirror.writable);
    }
    if (mirror.getter) {
      response =
          mirror.getter->buildRemoteObject(context, wrapMode, &remoteObject);
      if (!response.isSuccess()) return response;
      response =
          bindRemoteObjectIfNeeded(sessionId, context, mirror.getter->v8Value(),
                                   groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setGet(std::move(remoteObject));
    }
    if (mirror.setter) {
      response =
          mirror.setter->buildRemoteObject(context, wrapMode, &remoteObject);
      if (!response.isSuccess()) return response;
      response =
          bindRemoteObjectIfNeeded(sessionId, context, mirror.setter->v8Value(),
                                   groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setSet(std::move(remoteObject));
    }
    if (mirror.symbol) {
      response =
          mirror.symbol->buildRemoteObject(context, wrapMode, &remoteObject);
      if (!response.isSuccess()) return response;
      response =
          bindRemoteObjectIfNeeded(sessionId, context, mirror.symbol->v8Value(),
                                   groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setSymbol(std::move(remoteObject));
    }
    if (mirror.exception) {
      response =
          mirror.exception->buildRemoteObject(context, wrapMode, &remoteObject);
      if (!response.isSuccess()) return response;
      response = bindRemoteObjectIfNeeded(sessionId, context,
                                          mirror.exception->v8Value(),
                                          groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setValue(std::move(remoteObject));
      descriptor->setWasThrown(true);
    }
    (*properties)->emplace_back(std::move(descriptor));
  }
  return Response::OK();
}

Response InjectedScript::getInternalAndPrivateProperties(
    v8::Local<v8::Value> value, const String16& groupName,
    std::unique_ptr<protocol::Array<InternalPropertyDescriptor>>*
        internalProperties,
    std::unique_ptr<protocol::Array<PrivatePropertyDescriptor>>*
        privateProperties) {
  *internalProperties = std::make_unique<Array<InternalPropertyDescriptor>>();
  *privateProperties = std::make_unique<Array<PrivatePropertyDescriptor>>();

  if (!value->IsObject()) return Response::OK();

  v8::Local<v8::Object> value_obj = value.As<v8::Object>();

  v8::Local<v8::Context> context = m_context->context();
  int sessionId = m_sessionId;
  std::vector<InternalPropertyMirror> internalPropertiesWrappers;
  ValueMirror::getInternalProperties(m_context->context(), value_obj,
                                     &internalPropertiesWrappers);
  for (const auto& internalProperty : internalPropertiesWrappers) {
    std::unique_ptr<RemoteObject> remoteObject;
    Response response = internalProperty.value->buildRemoteObject(
        m_context->context(), WrapMode::kNoPreview, &remoteObject);
    if (!response.isSuccess()) return response;
    response = bindRemoteObjectIfNeeded(sessionId, context,
                                        internalProperty.value->v8Value(),
                                        groupName, remoteObject.get());
    if (!response.isSuccess()) return response;
    (*internalProperties)
        ->emplace_back(InternalPropertyDescriptor::create()
                           .setName(internalProperty.name)
                           .setValue(std::move(remoteObject))
                           .build());
  }

  std::vector<PrivatePropertyMirror> privatePropertyWrappers =
      ValueMirror::getPrivateProperties(context, value_obj);
  for (const auto& privateProperty : privatePropertyWrappers) {
    std::unique_ptr<PrivatePropertyDescriptor> descriptor =
        PrivatePropertyDescriptor::create()
            .setName(privateProperty.name)
            .build();

    std::unique_ptr<RemoteObject> remoteObject;
    Response response;
    DCHECK((privateProperty.getter || privateProperty.setter) ^
           (!!privateProperty.value));
    if (privateProperty.value) {
      response = privateProperty.value->buildRemoteObject(
          context, WrapMode::kNoPreview, &remoteObject);
      if (!response.isSuccess()) return response;
      response = bindRemoteObjectIfNeeded(sessionId, context,
                                          privateProperty.value->v8Value(),
                                          groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setValue(std::move(remoteObject));
    }

    if (privateProperty.getter) {
      response = privateProperty.getter->buildRemoteObject(
          context, WrapMode::kNoPreview, &remoteObject);
      if (!response.isSuccess()) return response;
      response = bindRemoteObjectIfNeeded(sessionId, context,
                                          privateProperty.getter->v8Value(),
                                          groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setGet(std::move(remoteObject));
    }

    if (privateProperty.setter) {
      response = privateProperty.setter->buildRemoteObject(
          context, WrapMode::kNoPreview, &remoteObject);
      if (!response.isSuccess()) return response;
      response = bindRemoteObjectIfNeeded(sessionId, context,
                                          privateProperty.setter->v8Value(),
                                          groupName, remoteObject.get());
      if (!response.isSuccess()) return response;
      descriptor->setSet(std::move(remoteObject));
    }

    (*privateProperties)->emplace_back(std::move(descriptor));
  }
  return Response::OK();
}

void InjectedScript::releaseObject(const String16& objectId) {
  std::vector<uint8_t> cbor;
  v8_crdtp::json::ConvertJSONToCBOR(
      v8_crdtp::span<uint16_t>(objectId.characters16(), objectId.length()),
      &cbor);
  std::unique_ptr<protocol::Value> parsedObjectId =
      protocol::Value::parseBinary(cbor.data(), cbor.size());
  if (!parsedObjectId) return;
  protocol::DictionaryValue* object =
      protocol::DictionaryValue::cast(parsedObjectId.get());
  if (!object) return;
  int boundId = 0;
  if (!object->getInteger("id", &boundId)) return;
  unbindObject(boundId);
}

Response InjectedScript::wrapObject(
    v8::Local<v8::Value> value, const String16& groupName, WrapMode wrapMode,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result) {
  return wrapObject(value, groupName, wrapMode, v8::MaybeLocal<v8::Value>(),
                    kMaxCustomPreviewDepth, result);
}

Response InjectedScript::wrapObject(
    v8::Local<v8::Value> value, const String16& groupName, WrapMode wrapMode,
    v8::MaybeLocal<v8::Value> customPreviewConfig, int maxCustomPreviewDepth,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result) {
  v8::Local<v8::Context> context = m_context->context();
  v8::Context::Scope contextScope(context);
  std::unique_ptr<ValueMirror> mirror = ValueMirror::create(context, value);
  if (!mirror) return Response::InternalError();
  return wrapObjectMirror(*mirror, groupName, wrapMode, customPreviewConfig,
                          maxCustomPreviewDepth, result);
}

Response InjectedScript::wrapObjectMirror(
    const ValueMirror& mirror, const String16& groupName, WrapMode wrapMode,
    v8::MaybeLocal<v8::Value> customPreviewConfig, int maxCustomPreviewDepth,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result) {
  int customPreviewEnabled = m_customPreviewEnabled;
  int sessionId = m_sessionId;
  v8::Local<v8::Context> context = m_context->context();
  v8::Context::Scope contextScope(context);
  Response response = mirror.buildRemoteObject(context, wrapMode, result);
  if (!response.isSuccess()) return response;
  v8::Local<v8::Value> value = mirror.v8Value();
  response = bindRemoteObjectIfNeeded(sessionId, context, value, groupName,
                                      result->get());
  if (!response.isSuccess()) return response;
  if (customPreviewEnabled && value->IsObject()) {
    std::unique_ptr<protocol::Runtime::CustomPreview> customPreview;
    generateCustomPreview(sessionId, groupName, value.As<v8::Object>(),
                          customPreviewConfig, maxCustomPreviewDepth,
                          &customPreview);
    if (customPreview) (*result)->setCustomPreview(std::move(customPreview));
  }
  return Response::OK();
}

std::unique_ptr<protocol::Runtime::RemoteObject> InjectedScript::wrapTable(
    v8::Local<v8::Object> table, v8::MaybeLocal<v8::Array> maybeColumns) {
  using protocol::Runtime::RemoteObject;
  using protocol::Runtime::ObjectPreview;
  using protocol::Runtime::PropertyPreview;
  using protocol::Array;

  v8::Isolate* isolate = m_context->isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = m_context->context();

  std::unique_ptr<RemoteObject> remoteObject;
  Response response =
      wrapObject(table, "console", WrapMode::kNoPreview, &remoteObject);
  if (!remoteObject || !response.isSuccess()) return nullptr;

  auto mirror = ValueMirror::create(context, table);
  std::unique_ptr<ObjectPreview> preview;
  int limit = 1000;
  mirror->buildObjectPreview(context, true /* generatePreviewForTable */,
                             &limit, &limit, &preview);
  if (!preview) return nullptr;

  std::vector<String16> selectedColumns;
  std::unordered_set<String16> columnSet;
  v8::Local<v8::Array> v8Columns;
  if (maybeColumns.ToLocal(&v8Columns)) {
    for (uint32_t i = 0; i < v8Columns->Length(); ++i) {
      v8::Local<v8::Value> column;
      if (v8Columns->Get(context, i).ToLocal(&column) && column->IsString()) {
        String16 name = toProtocolString(isolate, column.As<v8::String>());
        if (columnSet.find(name) == columnSet.end()) {
          columnSet.insert(name);
          selectedColumns.push_back(name);
        }
      }
    }
  }
  if (!selectedColumns.empty()) {
    for (const std::unique_ptr<PropertyPreview>& column :
         *preview->getProperties()) {
      ObjectPreview* columnPreview = column->getValuePreview(nullptr);
      if (!columnPreview) continue;
      // Use raw pointer here since the lifetime of each PropertyPreview is
      // ensured by columnPreview. This saves an additional clone.
      std::unordered_map<String16, PropertyPreview*> columnMap;
      for (const std::unique_ptr<PropertyPreview>& property :
           *columnPreview->getProperties()) {
        if (columnSet.find(property->getName()) == columnSet.end()) continue;
        columnMap[property->getName()] = property.get();
      }
      auto filtered = std::make_unique<Array<PropertyPreview>>();
      for (const String16& column : selectedColumns) {
        if (columnMap.find(column) == columnMap.end()) continue;
        filtered->push_back(columnMap[column]->clone());
      }
      columnPreview->setProperties(std::move(filtered));
    }
  }
  remoteObject->setPreview(std::move(preview));
  return remoteObject;
}

void InjectedScript::addPromiseCallback(
    V8InspectorSessionImpl* session, v8::MaybeLocal<v8::Value> value,
    const String16& objectGroup, WrapMode wrapMode, bool replMode,
    std::unique_ptr<EvaluateCallback> callback) {
  if (value.IsEmpty()) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  v8::MicrotasksScope microtasksScope(m_context->isolate(),
                                      v8::MicrotasksScope::kRunMicrotasks);
  if (ProtocolPromiseHandler::add(session, m_context->context(),
                                  value.ToLocalChecked(),
                                  m_context->contextId(), objectGroup, wrapMode,
                                  replMode, callback.get())) {
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
  m_customPreviewEnabled = enabled;
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
      std::vector<uint8_t> json;
      v8_crdtp::json::ConvertCBORToJSON(
          v8_crdtp::SpanFrom(callArgument->getValue(nullptr)->Serialize()),
          &json);
      value =
          "(" +
          String16(reinterpret_cast<const char*>(json.data()), json.size()) +
          ")";
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

Response InjectedScript::addExceptionToDetails(
    v8::Local<v8::Value> exception,
    protocol::Runtime::ExceptionDetails* exceptionDetails,
    const String16& objectGroup) {
  if (exception.IsEmpty()) return Response::OK();
  std::unique_ptr<protocol::Runtime::RemoteObject> wrapped;
  Response response =
      wrapObject(exception, objectGroup,
                 exception->IsNativeError() ? WrapMode::kNoPreview
                                            : WrapMode::kWithPreview,
                 &wrapped);
  if (!response.isSuccess()) return response;
  exceptionDetails->setException(std::move(wrapped));
  return Response::OK();
}

Response InjectedScript::createExceptionDetails(
    const v8::TryCatch& tryCatch, const String16& objectGroup,
    Maybe<protocol::Runtime::ExceptionDetails>* result) {
  if (!tryCatch.HasCaught()) return Response::InternalError();
  v8::Local<v8::Message> message = tryCatch.Message();
  v8::Local<v8::Value> exception = tryCatch.Exception();
  return createExceptionDetails(message, exception, objectGroup, result);
}

Response InjectedScript::createExceptionDetails(
    v8::Local<v8::Message> message, v8::Local<v8::Value> exception,
    const String16& objectGroup,
    Maybe<protocol::Runtime::ExceptionDetails>* result) {
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
  Response response =
      addExceptionToDetails(exception, exceptionDetails.get(), objectGroup);
  if (!response.isSuccess()) return response;
  *result = std::move(exceptionDetails);
  return Response::OK();
}

Response InjectedScript::wrapEvaluateResult(
    v8::MaybeLocal<v8::Value> maybeResultValue, const v8::TryCatch& tryCatch,
    const String16& objectGroup, WrapMode wrapMode,
    std::unique_ptr<protocol::Runtime::RemoteObject>* result,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails) {
  v8::Local<v8::Value> resultValue;
  if (!tryCatch.HasCaught()) {
    if (!maybeResultValue.ToLocal(&resultValue))
      return Response::InternalError();
    Response response = wrapObject(resultValue, objectGroup, wrapMode, result);
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
        wrapObject(exception, objectGroup,
                   exception->IsNativeError() ? WrapMode::kNoPreview
                                              : WrapMode::kWithPreview,
                   result);
    if (!response.isSuccess()) return response;
    // We send exception in result for compatibility reasons, even though it's
    // accessible through exceptionDetails.exception.
    response = createExceptionDetails(tryCatch, objectGroup, exceptionDetails);
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

String16 InjectedScript::bindObject(v8::Local<v8::Value> value,
                                    const String16& groupName) {
  if (m_lastBoundObjectId <= 0) m_lastBoundObjectId = 1;
  int id = m_lastBoundObjectId++;
  m_idToWrappedObject[id].Reset(m_context->isolate(), value);
  m_idToWrappedObject[id].AnnotateStrongRetainer(kGlobalHandleLabel);
  if (!groupName.isEmpty() && id > 0) {
    m_idToObjectGroupName[id] = groupName;
    m_nameToObjectGroup[groupName].push_back(id);
  }
  // TODO(dgozman): get rid of "injectedScript" notion.
  return String16::concat(
      "{\"injectedScriptId\":", String16::fromInteger(m_context->contextId()),
      ",\"id\":", String16::fromInteger(id), "}");
}

// static
Response InjectedScript::bindRemoteObjectIfNeeded(
    int sessionId, v8::Local<v8::Context> context, v8::Local<v8::Value> value,
    const String16& groupName, protocol::Runtime::RemoteObject* remoteObject) {
  if (!remoteObject) return Response::OK();
  if (remoteObject->hasValue()) return Response::OK();
  if (remoteObject->hasUnserializableValue()) return Response::OK();
  if (remoteObject->getType() != RemoteObject::TypeEnum::Undefined) {
    v8::Isolate* isolate = context->GetIsolate();
    V8InspectorImpl* inspector =
        static_cast<V8InspectorImpl*>(v8::debug::GetInspector(isolate));
    InspectedContext* inspectedContext =
        inspector->getContext(InspectedContext::contextId(context));
    InjectedScript* injectedScript =
        inspectedContext ? inspectedContext->getInjectedScript(sessionId)
                         : nullptr;
    if (!injectedScript) {
      return Response::Error("Cannot find context with specified id");
    }
    remoteObject->setObjectId(injectedScript->bindObject(value, groupName));
  }
  return Response::OK();
}

void InjectedScript::unbindObject(int id) {
  m_idToWrappedObject.erase(id);
  m_idToObjectGroupName.erase(id);
}

}  // namespace v8_inspector
