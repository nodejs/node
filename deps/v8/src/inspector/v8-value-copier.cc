// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-value-copier.h"

namespace v8_inspector {

namespace {

static int kMaxDepth = 20;
static int kMaxCalls = 1000;

class V8ValueCopier {
 public:
  v8::MaybeLocal<v8::Value> copy(v8::Local<v8::Value> value, int depth) {
    if (++m_calls > kMaxCalls || depth > kMaxDepth)
      return v8::MaybeLocal<v8::Value>();

    if (value.IsEmpty()) return v8::MaybeLocal<v8::Value>();
    if (value->IsNull() || value->IsUndefined() || value->IsBoolean() ||
        value->IsString() || value->IsNumber())
      return value;
    if (!value->IsObject()) return v8::MaybeLocal<v8::Value>();
    v8::Local<v8::Object> object = value.As<v8::Object>();
    if (object->CreationContext() != m_from) return value;

    if (object->IsArray()) {
      v8::Local<v8::Array> array = object.As<v8::Array>();
      v8::Local<v8::Array> result = v8::Array::New(m_isolate, array->Length());
      if (!result->SetPrototype(m_to, v8::Null(m_isolate)).FromMaybe(false))
        return v8::MaybeLocal<v8::Value>();
      for (uint32_t i = 0; i < array->Length(); ++i) {
        v8::Local<v8::Value> item;
        if (!array->Get(m_from, i).ToLocal(&item))
          return v8::MaybeLocal<v8::Value>();
        v8::Local<v8::Value> copied;
        if (!copy(item, depth + 1).ToLocal(&copied))
          return v8::MaybeLocal<v8::Value>();
        if (!createDataProperty(m_to, result, i, copied).FromMaybe(false))
          return v8::MaybeLocal<v8::Value>();
      }
      return result;
    }

    v8::Local<v8::Object> result = v8::Object::New(m_isolate);
    if (!result->SetPrototype(m_to, v8::Null(m_isolate)).FromMaybe(false))
      return v8::MaybeLocal<v8::Value>();
    v8::Local<v8::Array> properties;
    if (!object->GetOwnPropertyNames(m_from).ToLocal(&properties))
      return v8::MaybeLocal<v8::Value>();
    for (uint32_t i = 0; i < properties->Length(); ++i) {
      v8::Local<v8::Value> name;
      if (!properties->Get(m_from, i).ToLocal(&name) || !name->IsString())
        return v8::MaybeLocal<v8::Value>();
      v8::Local<v8::Value> property;
      if (!object->Get(m_from, name).ToLocal(&property))
        return v8::MaybeLocal<v8::Value>();
      v8::Local<v8::Value> copied;
      if (!copy(property, depth + 1).ToLocal(&copied))
        return v8::MaybeLocal<v8::Value>();
      if (!createDataProperty(m_to, result, v8::Local<v8::String>::Cast(name),
                              copied)
               .FromMaybe(false))
        return v8::MaybeLocal<v8::Value>();
    }
    return result;
  }

  v8::Isolate* m_isolate;
  v8::Local<v8::Context> m_from;
  v8::Local<v8::Context> m_to;
  int m_calls;
};

protocol::Response toProtocolValue(v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value, int maxDepth,
                                   std::unique_ptr<protocol::Value>* result) {
  using protocol::Response;
  if (value.IsEmpty()) {
    UNREACHABLE();
    return Response::InternalError();
  }

  if (!maxDepth) return Response::Error("Object reference chain is too long");
  maxDepth--;

  if (value->IsNull() || value->IsUndefined()) {
    *result = protocol::Value::null();
    return Response::OK();
  }
  if (value->IsBoolean()) {
    *result =
        protocol::FundamentalValue::create(value.As<v8::Boolean>()->Value());
    return Response::OK();
  }
  if (value->IsNumber()) {
    double doubleValue = value.As<v8::Number>()->Value();
    int intValue = static_cast<int>(doubleValue);
    if (intValue == doubleValue) {
      *result = protocol::FundamentalValue::create(intValue);
      return Response::OK();
    }
    *result = protocol::FundamentalValue::create(doubleValue);
    return Response::OK();
  }
  if (value->IsString()) {
    *result =
        protocol::StringValue::create(toProtocolString(value.As<v8::String>()));
    return Response::OK();
  }
  if (value->IsArray()) {
    v8::Local<v8::Array> array = value.As<v8::Array>();
    std::unique_ptr<protocol::ListValue> inspectorArray =
        protocol::ListValue::create();
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; i++) {
      v8::Local<v8::Value> value;
      if (!array->Get(context, i).ToLocal(&value))
        return Response::InternalError();
      std::unique_ptr<protocol::Value> element;
      Response response = toProtocolValue(context, value, maxDepth, &element);
      if (!response.isSuccess()) return response;
      inspectorArray->pushValue(std::move(element));
    }
    *result = std::move(inspectorArray);
    return Response::OK();
  }
  if (value->IsObject()) {
    std::unique_ptr<protocol::DictionaryValue> jsonObject =
        protocol::DictionaryValue::create();
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
    v8::Local<v8::Array> propertyNames;
    if (!object->GetPropertyNames(context).ToLocal(&propertyNames))
      return Response::InternalError();
    uint32_t length = propertyNames->Length();
    for (uint32_t i = 0; i < length; i++) {
      v8::Local<v8::Value> name;
      if (!propertyNames->Get(context, i).ToLocal(&name))
        return Response::InternalError();
      // FIXME(yurys): v8::Object should support GetOwnPropertyNames
      if (name->IsString()) {
        v8::Maybe<bool> hasRealNamedProperty = object->HasRealNamedProperty(
            context, v8::Local<v8::String>::Cast(name));
        if (!hasRealNamedProperty.IsJust() || !hasRealNamedProperty.FromJust())
          continue;
      }
      v8::Local<v8::String> propertyName;
      if (!name->ToString(context).ToLocal(&propertyName)) continue;
      v8::Local<v8::Value> property;
      if (!object->Get(context, name).ToLocal(&property))
        return Response::InternalError();
      std::unique_ptr<protocol::Value> propertyValue;
      Response response =
          toProtocolValue(context, property, maxDepth, &propertyValue);
      if (!response.isSuccess()) return response;
      jsonObject->setValue(toProtocolString(propertyName),
                           std::move(propertyValue));
    }
    *result = std::move(jsonObject);
    return Response::OK();
  }
  return Response::Error("Object couldn't be returned by value");
}

}  // namespace

v8::MaybeLocal<v8::Value> copyValueFromDebuggerContext(
    v8::Isolate* isolate, v8::Local<v8::Context> debuggerContext,
    v8::Local<v8::Context> toContext, v8::Local<v8::Value> value) {
  V8ValueCopier copier;
  copier.m_isolate = isolate;
  copier.m_from = debuggerContext;
  copier.m_to = toContext;
  copier.m_calls = 0;
  return copier.copy(value, 0);
}

v8::Maybe<bool> createDataProperty(v8::Local<v8::Context> context,
                                   v8::Local<v8::Object> object,
                                   v8::Local<v8::Name> key,
                                   v8::Local<v8::Value> value) {
  v8::TryCatch tryCatch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return object->CreateDataProperty(context, key, value);
}

v8::Maybe<bool> createDataProperty(v8::Local<v8::Context> context,
                                   v8::Local<v8::Array> array, int index,
                                   v8::Local<v8::Value> value) {
  v8::TryCatch tryCatch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return array->CreateDataProperty(context, index, value);
}

protocol::Response toProtocolValue(v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value,
                                   std::unique_ptr<protocol::Value>* result) {
  return toProtocolValue(context, value, 1000, result);
}

}  // namespace v8_inspector
