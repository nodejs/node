// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-value-utils.h"

namespace v8_inspector {

namespace {

protocol::Response toProtocolValue(v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value, int maxDepth,
                                   std::unique_ptr<protocol::Value>* result) {
  using protocol::Response;
  if (value.IsEmpty()) {
    UNREACHABLE();
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
    *result = protocol::StringValue::create(
        toProtocolString(context->GetIsolate(), value.As<v8::String>()));
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
        if (hasRealNamedProperty.IsNothing() ||
            !hasRealNamedProperty.FromJust())
          continue;
      }
      v8::Local<v8::String> propertyName;
      if (!name->ToString(context).ToLocal(&propertyName)) continue;
      v8::Local<v8::Value> property;
      if (!object->Get(context, name).ToLocal(&property))
        return Response::InternalError();
      if (property->IsUndefined()) continue;
      std::unique_ptr<protocol::Value> propertyValue;
      Response response =
          toProtocolValue(context, property, maxDepth, &propertyValue);
      if (!response.isSuccess()) return response;
      jsonObject->setValue(
          toProtocolString(context->GetIsolate(), propertyName),
          std::move(propertyValue));
    }
    *result = std::move(jsonObject);
    return Response::OK();
  }
  return Response::Error("Object couldn't be returned by value");
}

}  // namespace

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
