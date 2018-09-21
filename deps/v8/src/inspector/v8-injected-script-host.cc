// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-injected-script-host.h"

#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/injected-script.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-internal-value-type.h"
#include "src/inspector/v8-value-utils.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace {

void setFunctionProperty(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> obj, const char* name,
                         v8::FunctionCallback callback,
                         v8::Local<v8::External> external) {
  v8::Local<v8::String> funcName =
      toV8StringInternalized(context->GetIsolate(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, external, 0,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&func))
    return;
  func->SetName(funcName);
  createDataProperty(context, obj, funcName, func);
}

V8InspectorImpl* unwrapInspector(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(!info.Data().IsEmpty());
  DCHECK(info.Data()->IsExternal());
  V8InspectorImpl* inspector =
      static_cast<V8InspectorImpl*>(info.Data().As<v8::External>()->Value());
  DCHECK(inspector);
  return inspector;
}

template <typename TypedArray>
void addTypedArrayProperty(std::vector<v8::Local<v8::Value>>* props,
                           v8::Isolate* isolate,
                           v8::Local<v8::ArrayBuffer> arraybuffer,
                           String16 name, size_t length) {
  props->push_back(toV8String(isolate, name));
  props->push_back(TypedArray::New(arraybuffer, 0, length));
}

}  // namespace

v8::Local<v8::Object> V8InjectedScriptHost::create(
    v8::Local<v8::Context> context, V8InspectorImpl* inspector) {
  v8::Isolate* isolate = inspector->isolate();
  v8::Local<v8::Object> injectedScriptHost = v8::Object::New(isolate);
  bool success = injectedScriptHost->SetPrototype(context, v8::Null(isolate))
                     .FromMaybe(false);
  DCHECK(success);
  USE(success);
  v8::Local<v8::External> debuggerExternal =
      v8::External::New(isolate, inspector);
  setFunctionProperty(context, injectedScriptHost, "nullifyPrototype",
                      V8InjectedScriptHost::nullifyPrototypeCallback,
                      debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "getProperty",
                      V8InjectedScriptHost::getPropertyCallback,
                      debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "internalConstructorName",
                      V8InjectedScriptHost::internalConstructorNameCallback,
                      debuggerExternal);
  setFunctionProperty(
      context, injectedScriptHost, "formatAccessorsAsProperties",
      V8InjectedScriptHost::formatAccessorsAsProperties, debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "subtype",
                      V8InjectedScriptHost::subtypeCallback, debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "getInternalProperties",
                      V8InjectedScriptHost::getInternalPropertiesCallback,
                      debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "objectHasOwnProperty",
                      V8InjectedScriptHost::objectHasOwnPropertyCallback,
                      debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "bind",
                      V8InjectedScriptHost::bindCallback, debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "proxyTargetValue",
                      V8InjectedScriptHost::proxyTargetValueCallback,
                      debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "nativeAccessorDescriptor",
                      V8InjectedScriptHost::nativeAccessorDescriptorCallback,
                      debuggerExternal);
  setFunctionProperty(context, injectedScriptHost, "typedArrayProperties",
                      V8InjectedScriptHost::typedArrayPropertiesCallback,
                      debuggerExternal);
  createDataProperty(context, injectedScriptHost,
                     toV8StringInternalized(isolate, "keys"),
                     v8::debug::GetBuiltin(isolate, v8::debug::kObjectKeys));
  createDataProperty(
      context, injectedScriptHost,
      toV8StringInternalized(isolate, "getPrototypeOf"),
      v8::debug::GetBuiltin(isolate, v8::debug::kObjectGetPrototypeOf));
  createDataProperty(
      context, injectedScriptHost,
      toV8StringInternalized(isolate, "getOwnPropertyDescriptor"),
      v8::debug::GetBuiltin(isolate,
                            v8::debug::kObjectGetOwnPropertyDescriptor));
  createDataProperty(
      context, injectedScriptHost,
      toV8StringInternalized(isolate, "getOwnPropertyNames"),
      v8::debug::GetBuiltin(isolate, v8::debug::kObjectGetOwnPropertyNames));
  createDataProperty(
      context, injectedScriptHost,
      toV8StringInternalized(isolate, "getOwnPropertySymbols"),
      v8::debug::GetBuiltin(isolate, v8::debug::kObjectGetOwnPropertySymbols));
  return injectedScriptHost;
}

void V8InjectedScriptHost::nullifyPrototypeCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK_EQ(1, info.Length());
  DCHECK(info[0]->IsObject());
  if (!info[0]->IsObject()) return;
  v8::Isolate* isolate = info.GetIsolate();
  info[0]
      .As<v8::Object>()
      ->SetPrototype(isolate->GetCurrentContext(), v8::Null(isolate))
      .ToChecked();
}

void V8InjectedScriptHost::getPropertyCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.Length() == 2 && info[1]->IsString());
  if (!info[0]->IsObject()) return;
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::TryCatch tryCatch(isolate);
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  v8::Local<v8::Value> property;
  if (info[0]
          .As<v8::Object>()
          ->Get(context, v8::Local<v8::String>::Cast(info[1]))
          .ToLocal(&property)) {
    info.GetReturnValue().Set(property);
  }
}

void V8InjectedScriptHost::internalConstructorNameCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1 || !info[0]->IsObject()) return;

  v8::Local<v8::Object> object = info[0].As<v8::Object>();
  info.GetReturnValue().Set(object->GetConstructorName());
}

void V8InjectedScriptHost::formatAccessorsAsProperties(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK_EQ(info.Length(), 2);
  info.GetReturnValue().Set(false);
  if (!info[1]->IsFunction()) return;
  // Check that function is user-defined.
  if (info[1].As<v8::Function>()->ScriptId() != v8::UnboundScript::kNoScriptId)
    return;
  info.GetReturnValue().Set(
      unwrapInspector(info)->client()->formatAccessorsAsProperties(info[0]));
}

void V8InjectedScriptHost::subtypeCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1) return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value = info[0];
  if (value->IsObject()) {
    v8::Local<v8::Value> internalType = v8InternalValueTypeFrom(
        isolate->GetCurrentContext(), v8::Local<v8::Object>::Cast(value));
    if (internalType->IsString()) {
      info.GetReturnValue().Set(internalType);
      return;
    }
  }
  if (value->IsArray() || value->IsArgumentsObject()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "array"));
    return;
  }
  if (value->IsTypedArray()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "typedarray"));
    return;
  }
  if (value->IsDate()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "date"));
    return;
  }
  if (value->IsRegExp()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "regexp"));
    return;
  }
  if (value->IsMap()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "map"));
    return;
  }
  if (value->IsWeakMap()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "weakmap"));
    return;
  }
  if (value->IsSet()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "set"));
    return;
  }
  if (value->IsWeakSet()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "weakset"));
    return;
  }
  if (value->IsMapIterator() || value->IsSetIterator()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "iterator"));
    return;
  }
  if (value->IsGeneratorObject()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "generator"));
    return;
  }
  if (value->IsNativeError()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "error"));
    return;
  }
  if (value->IsProxy()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "proxy"));
    return;
  }
  if (value->IsPromise()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "promise"));
    return;
  }
  if (value->IsArrayBuffer() || value->IsSharedArrayBuffer()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "arraybuffer"));
    return;
  }
  if (value->IsDataView()) {
    info.GetReturnValue().Set(toV8StringInternalized(isolate, "dataview"));
    return;
  }
  std::unique_ptr<StringBuffer> subtype =
      unwrapInspector(info)->client()->valueSubtype(value);
  if (subtype) {
    info.GetReturnValue().Set(toV8String(isolate, subtype->string()));
    return;
  }
}

void V8InjectedScriptHost::getInternalPropertiesCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1) return;

  std::unordered_set<String16> allowedProperties;
  if (info[0]->IsBooleanObject() || info[0]->IsNumberObject() ||
      info[0]->IsStringObject() || info[0]->IsSymbolObject() ||
      info[0]->IsBigIntObject()) {
    allowedProperties.insert(String16("[[PrimitiveValue]]"));
  } else if (info[0]->IsPromise()) {
    allowedProperties.insert(String16("[[PromiseStatus]]"));
    allowedProperties.insert(String16("[[PromiseValue]]"));
  } else if (info[0]->IsGeneratorObject()) {
    allowedProperties.insert(String16("[[GeneratorStatus]]"));
  } else if (info[0]->IsMap() || info[0]->IsWeakMap() || info[0]->IsSet() ||
             info[0]->IsWeakSet() || info[0]->IsMapIterator() ||
             info[0]->IsSetIterator()) {
    allowedProperties.insert(String16("[[Entries]]"));
  }
  if (!allowedProperties.size()) return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Array> allProperties;
  if (!unwrapInspector(info)
           ->debugger()
           ->internalProperties(isolate->GetCurrentContext(), info[0])
           .ToLocal(&allProperties) ||
      !allProperties->IsArray() || allProperties->Length() % 2 != 0)
    return;

  {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::TryCatch tryCatch(isolate);
    v8::Isolate::DisallowJavascriptExecutionScope throwJs(
        isolate,
        v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);

    v8::Local<v8::Array> properties = v8::Array::New(isolate);
    if (tryCatch.HasCaught()) return;

    uint32_t outputIndex = 0;
    for (uint32_t i = 0; i < allProperties->Length(); i += 2) {
      v8::Local<v8::Value> key;
      if (!allProperties->Get(context, i).ToLocal(&key)) continue;
      if (tryCatch.HasCaught()) {
        tryCatch.Reset();
        continue;
      }
      String16 keyString = toProtocolStringWithTypeCheck(isolate, key);
      if (keyString.isEmpty() ||
          allowedProperties.find(keyString) == allowedProperties.end())
        continue;
      v8::Local<v8::Value> value;
      if (!allProperties->Get(context, i + 1).ToLocal(&value)) continue;
      if (tryCatch.HasCaught()) {
        tryCatch.Reset();
        continue;
      }
      createDataProperty(context, properties, outputIndex++, key);
      createDataProperty(context, properties, outputIndex++, value);
    }
    info.GetReturnValue().Set(properties);
  }
}

void V8InjectedScriptHost::objectHasOwnPropertyCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 2 || !info[0]->IsObject() || !info[1]->IsString()) return;
  bool result = info[0]
                    .As<v8::Object>()
                    ->HasOwnProperty(info.GetIsolate()->GetCurrentContext(),
                                     v8::Local<v8::String>::Cast(info[1]))
                    .FromMaybe(false);
  info.GetReturnValue().Set(v8::Boolean::New(info.GetIsolate(), result));
}

void V8InjectedScriptHost::bindCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 2 || !info[1]->IsString()) return;
  InjectedScript* injectedScript =
      InjectedScript::fromInjectedScriptHost(info.GetIsolate(), info.Holder());
  if (!injectedScript) return;

  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::String> v8groupName =
      info[1]->ToString(context).ToLocalChecked();
  String16 groupName =
      toProtocolStringWithTypeCheck(info.GetIsolate(), v8groupName);
  int id = injectedScript->bindObject(info[0], groupName);
  info.GetReturnValue().Set(id);
}

void V8InjectedScriptHost::proxyTargetValueCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() != 1 || !info[0]->IsProxy()) {
    UNREACHABLE();
    return;
  }
  v8::Local<v8::Value> target = info[0].As<v8::Proxy>();
  while (target->IsProxy())
    target = v8::Local<v8::Proxy>::Cast(target)->GetTarget();
  info.GetReturnValue().Set(target);
}

void V8InjectedScriptHost::nativeAccessorDescriptorCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (info.Length() != 2 || !info[0]->IsObject() || !info[1]->IsName()) {
    info.GetReturnValue().Set(v8::Undefined(isolate));
    return;
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  int flags = v8::debug::GetNativeAccessorDescriptor(
      context, v8::Local<v8::Object>::Cast(info[0]),
      v8::Local<v8::Name>::Cast(info[1]));
  if (flags == static_cast<int>(v8::debug::NativeAccessorType::None)) {
    info.GetReturnValue().Set(v8::Undefined(isolate));
    return;
  }

  bool isBuiltin =
      flags & static_cast<int>(v8::debug::NativeAccessorType::IsBuiltin);
  bool hasGetter =
      flags & static_cast<int>(v8::debug::NativeAccessorType::HasGetter);
  bool hasSetter =
      flags & static_cast<int>(v8::debug::NativeAccessorType::HasSetter);
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result->SetPrototype(context, v8::Null(isolate)).ToChecked();
  createDataProperty(context, result, toV8String(isolate, "isBuiltin"),
                     v8::Boolean::New(isolate, isBuiltin));
  createDataProperty(context, result, toV8String(isolate, "hasGetter"),
                     v8::Boolean::New(isolate, hasGetter));
  createDataProperty(context, result, toV8String(isolate, "hasSetter"),
                     v8::Boolean::New(isolate, hasSetter));
  info.GetReturnValue().Set(result);
}

void V8InjectedScriptHost::typedArrayPropertiesCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (info.Length() != 1 || !info[0]->IsArrayBuffer()) return;

  v8::TryCatch tryCatch(isolate);
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  v8::Local<v8::ArrayBuffer> arrayBuffer = info[0].As<v8::ArrayBuffer>();
  size_t length = arrayBuffer->ByteLength();
  if (length == 0) return;
  std::vector<v8::Local<v8::Value>> arrays_vector;
  addTypedArrayProperty<v8::Int8Array>(&arrays_vector, isolate, arrayBuffer,
                                       "[[Int8Array]]", length);
  addTypedArrayProperty<v8::Uint8Array>(&arrays_vector, isolate, arrayBuffer,
                                        "[[Uint8Array]]", length);

  if (length % 2 == 0) {
    addTypedArrayProperty<v8::Int16Array>(&arrays_vector, isolate, arrayBuffer,
                                          "[[Int16Array]]", length / 2);
  }
  if (length % 4 == 0) {
    addTypedArrayProperty<v8::Int32Array>(&arrays_vector, isolate, arrayBuffer,
                                          "[[Int32Array]]", length / 4);
  }

  if (tryCatch.HasCaught()) return;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> arrays =
      v8::Array::New(isolate, static_cast<uint32_t>(arrays_vector.size()));
  for (uint32_t i = 0; i < static_cast<uint32_t>(arrays_vector.size()); i++)
    createDataProperty(context, arrays, i, arrays_vector[i]);
  if (tryCatch.HasCaught()) return;
  info.GetReturnValue().Set(arrays);
}

}  // namespace v8_inspector
