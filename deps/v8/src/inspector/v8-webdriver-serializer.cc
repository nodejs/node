// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-webdriver-serializer.h"

#include "include/v8-container.h"
#include "include/v8-context.h"
#include "include/v8-date.h"
#include "include/v8-exception.h"
#include "include/v8-regexp.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/value-mirror.h"

namespace v8_inspector {

using protocol::Response;
// private
protocol::Response _serializeRecursively(
    v8::Local<v8::Value> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Value>* result) {
  std::unique_ptr<ValueMirror> mirror = ValueMirror::create(context, value);
  std::unique_ptr<protocol::Runtime::WebDriverValue> webDriver_value;
  Response response =
      mirror->buildWebDriverValue(context, max_depth - 1, &webDriver_value);
  if (!response.IsSuccess()) return response;
  if (!webDriver_value) return Response::InternalError();

  std::unique_ptr<protocol::DictionaryValue> result_dict =
      protocol::DictionaryValue::create();

  result_dict->setValue(
      protocol::String("type"),
      protocol::StringValue::create(webDriver_value->getType()));
  if (webDriver_value->hasValue())
    result_dict->setValue(protocol::String("value"),
                          webDriver_value->getValue(nullptr)->clone());

  (*result) = std::move(result_dict);
  return Response::Success();
}

String16 descriptionForObject(v8::Isolate* isolate,
                              v8::Local<v8::Object> object) {
  return toProtocolString(isolate, object->GetConstructorName());
}

String16 descriptionForDate(v8::Local<v8::Context> context,
                            v8::Local<v8::Date> date) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::String> description;
  if (!date->ToString(context).ToLocal(&description)) {
    return descriptionForObject(isolate, date);
  }
  return toProtocolString(isolate, description);
}

String16 _descriptionForRegExpFlags(v8::Local<v8::RegExp> value) {
  String16Builder result_string_builder;
  v8::RegExp::Flags flags = value->GetFlags();
  if (flags & v8::RegExp::Flags::kHasIndices) result_string_builder.append('d');
  if (flags & v8::RegExp::Flags::kGlobal) result_string_builder.append('g');
  if (flags & v8::RegExp::Flags::kIgnoreCase) result_string_builder.append('i');
  if (flags & v8::RegExp::Flags::kLinear) result_string_builder.append('l');
  if (flags & v8::RegExp::Flags::kMultiline) result_string_builder.append('m');
  if (flags & v8::RegExp::Flags::kDotAll) result_string_builder.append('s');
  if (flags & v8::RegExp::Flags::kUnicode) result_string_builder.append('u');
  if (flags & v8::RegExp::Flags::kSticky) result_string_builder.append('y');
  return result_string_builder.toString();
}

protocol::Response _serializeRegexp(
    v8::Local<v8::RegExp> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  *result = protocol::Runtime::WebDriverValue::create()
                .setType(protocol::Runtime::WebDriverValue::TypeEnum::Regexp)
                .build();

  std::unique_ptr<protocol::DictionaryValue> result_value =
      protocol::DictionaryValue::create();

  result_value->setValue(protocol::String("pattern"),
                         protocol::StringValue::create(toProtocolString(
                             context->GetIsolate(), value->GetSource())));

  String16 flags = _descriptionForRegExpFlags(value);
  if (!flags.isEmpty())
    result_value->setValue(protocol::String("flags"),
                           protocol::StringValue::create(flags));

  (*result)->setValue(std::move(result_value));
  return Response::Success();
}

protocol::Response _serializeDate(
    v8::Local<v8::Date> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  *result = protocol::Runtime::WebDriverValue::create()
                .setType(protocol::Runtime::WebDriverValue::TypeEnum::Date)
                .build();

  (*result)->setValue(protocol::StringValue::create(
      descriptionForDate(context, value.As<v8::Date>())));
  return Response::Success();
}

protocol::Response _serializeArrayValue(
    v8::Local<v8::Array> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Value>* result) {
  std::unique_ptr<protocol::ListValue> result_value =
      protocol::ListValue::create();
  uint32_t length = value->Length();
  for (uint32_t i = 0; i < length; i++) {
    v8::Local<v8::Value> element_value;
    std::unique_ptr<protocol::Value> element_protocol_value;
    if (!value->Get(context, i).ToLocal(&element_value))
      return Response::InternalError();

    Response response = _serializeRecursively(element_value, context, max_depth,
                                              &element_protocol_value);
    if (!response.IsSuccess()) return response;

    result_value->pushValue(std::move(element_protocol_value));
  }
  *result = std::move(result_value);
  return Response::Success();
}

protocol::Response _serializeArray(
    v8::Local<v8::Array> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  *result = protocol::Runtime::WebDriverValue::create()
                .setType(protocol::Runtime::WebDriverValue::TypeEnum::Array)
                .build();

  if (max_depth <= 0) return Response::Success();

  std::unique_ptr<protocol::Value> result_value;
  Response response =
      _serializeArrayValue(value, context, max_depth, &result_value);
  if (!response.IsSuccess()) return response;

  (*result)->setValue(std::move(result_value));
  return Response::Success();
}

protocol::Response _serializeMap(
    v8::Local<v8::Map> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  *result = protocol::Runtime::WebDriverValue::create()
                .setType(protocol::Runtime::WebDriverValue::TypeEnum::Map)
                .build();

  if (max_depth <= 0) return Response::Success();

  std::unique_ptr<protocol::ListValue> result_value =
      protocol::ListValue::create();

  v8::Local<v8::Array> properties_and_values = value->AsArray();

  uint32_t length = properties_and_values->Length();
  for (uint32_t i = 0; i < length; i += 2) {
    v8::Local<v8::Value> key_value, property_value;
    std::unique_ptr<protocol::Value> key_protocol_value,
        property_protocol_value;

    if (!properties_and_values->Get(context, i).ToLocal(&key_value))
      return Response::InternalError();
    if (!properties_and_values->Get(context, i + 1).ToLocal(&property_value))
      return Response::InternalError();
    if (property_value->IsUndefined()) continue;

    if (key_value->IsString()) {
      key_protocol_value = protocol::StringValue::create(
          toProtocolString(context->GetIsolate(), key_value.As<v8::String>()));
    } else {
      Response response = _serializeRecursively(key_value, context, max_depth,
                                                &key_protocol_value);
      if (!response.IsSuccess()) return response;
    }

    Response response = _serializeRecursively(
        property_value, context, max_depth, &property_protocol_value);
    if (!response.IsSuccess()) return response;

    std::unique_ptr<protocol::ListValue> value_list =
        protocol::ListValue::create();

    // command->pushValue(protocol::StringValue::create(method));
    value_list->pushValue(std::move(key_protocol_value));
    value_list->pushValue(std::move(property_protocol_value));

    result_value->pushValue(std::move(value_list));
  }

  (*result)->setValue(std::move(result_value));
  return Response::Success();
}

protocol::Response _serializeSet(
    v8::Local<v8::Set> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  *result = protocol::Runtime::WebDriverValue::create()
                .setType(protocol::Runtime::WebDriverValue::TypeEnum::Set)
                .build();

  if (max_depth <= 0) return Response::Success();

  std::unique_ptr<protocol::Value> result_value;
  Response response =
      _serializeArrayValue(value->AsArray(), context, max_depth, &result_value);
  if (!response.IsSuccess()) return response;

  (*result)->setValue(std::move(result_value));
  return Response::Success();
}

protocol::Response _serializeObjectValue(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Value>* result) {
  std::unique_ptr<protocol::ListValue> result_list =
      protocol::ListValue::create();
  // Iterate through object's properties.
  v8::Local<v8::Array> property_names;
  if (!value->GetOwnPropertyNames(context).ToLocal(&property_names))
    return Response::InternalError();
  uint32_t length = property_names->Length();
  for (uint32_t i = 0; i < length; i++) {
    v8::Local<v8::Value> key_value, property_value;
    std::unique_ptr<protocol::Value> key_protocol_value,
        property_protocol_value;

    if (!property_names->Get(context, i).ToLocal(&key_value))
      return Response::InternalError();

    if (key_value->IsString()) {
      v8::Maybe<bool> hasRealNamedProperty =
          value->HasRealNamedProperty(context, key_value.As<v8::String>());
      // Don't access properties with interceptors.
      if (hasRealNamedProperty.IsNothing() || !hasRealNamedProperty.FromJust())
        continue;
      key_protocol_value = protocol::StringValue::create(
          toProtocolString(context->GetIsolate(), key_value.As<v8::String>()));
    } else {
      Response response = _serializeRecursively(key_value, context, max_depth,
                                                &key_protocol_value);
      if (!response.IsSuccess()) return response;
    }

    if (!value->Get(context, key_value).ToLocal(&property_value))
      return Response::InternalError();
    if (property_value->IsUndefined()) continue;

    Response response = _serializeRecursively(
        property_value, context, max_depth, &property_protocol_value);
    if (!response.IsSuccess()) return response;

    std::unique_ptr<protocol::ListValue> value_list =
        protocol::ListValue::create();

    value_list->pushValue(std::move(key_protocol_value));
    value_list->pushValue(std::move(property_protocol_value));

    result_list->pushValue(std::move(value_list));
  }
  (*result) = std::move(result_list);
  return Response::Success();
}

protocol::Response _serializeObject(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  *result = protocol::Runtime::WebDriverValue::create()
                .setType(protocol::Runtime::WebDriverValue::TypeEnum::Object)
                .build();

  if (max_depth <= 0) return Response::Success();

  std::unique_ptr<protocol::Value> result_value;
  Response response = _serializeObjectValue(value.As<v8::Object>(), context,
                                            max_depth, &result_value);
  if (!response.IsSuccess()) return response;

  (*result)->setValue(std::move(result_value));
  return Response::Success();
}

protocol::Response V8WebDriverSerializer::serializeV8Value(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context, int max_depth,
    std::unique_ptr<protocol::Runtime::WebDriverValue>* result) {
  if (value->IsArray()) {
    Response response =
        _serializeArray(value.As<v8::Array>(), context, max_depth, result);
    return response;
  }
  if (value->IsRegExp()) {
    Response response =
        _serializeRegexp(value.As<v8::RegExp>(), context, max_depth, result);
    return response;
  }
  if (value->IsDate()) {
    Response response =
        _serializeDate(value.As<v8::Date>(), context, max_depth, result);
    return response;
  }
  if (value->IsMap()) {
    Response response =
        _serializeMap(value.As<v8::Map>(), context, max_depth, result);
    return response;
  }
  if (value->IsSet()) {
    Response response =
        _serializeSet(value.As<v8::Set>(), context, max_depth, result);
    return response;
  }
  if (value->IsWeakMap()) {
    *result = protocol::Runtime::WebDriverValue::create()
                  .setType(protocol::Runtime::WebDriverValue::TypeEnum::Weakmap)
                  .build();
    return Response::Success();
  }
  if (value->IsWeakSet()) {
    *result = protocol::Runtime::WebDriverValue::create()
                  .setType(protocol::Runtime::WebDriverValue::TypeEnum::Weakset)
                  .build();
    return Response::Success();
  }
  if (value->IsNativeError()) {
    *result = protocol::Runtime::WebDriverValue::create()
                  .setType(protocol::Runtime::WebDriverValue::TypeEnum::Error)
                  .build();
    return Response::Success();
  }
  if (value->IsProxy()) {
    *result = protocol::Runtime::WebDriverValue::create()
                  .setType(protocol::Runtime::WebDriverValue::TypeEnum::Proxy)
                  .build();
    return Response::Success();
  }
  if (value->IsPromise()) {
    *result = protocol::Runtime::WebDriverValue::create()
                  .setType(protocol::Runtime::WebDriverValue::TypeEnum::Promise)
                  .build();
    return Response::Success();
  }
  if (value->IsTypedArray()) {
    *result =
        protocol::Runtime::WebDriverValue::create()
            .setType(protocol::Runtime::WebDriverValue::TypeEnum::Typedarray)
            .build();
    return Response::Success();
  }
  if (value->IsArrayBuffer()) {
    *result =
        protocol::Runtime::WebDriverValue::create()
            .setType(protocol::Runtime::WebDriverValue::TypeEnum::Arraybuffer)
            .build();
    return Response::Success();
  }
  if (value->IsFunction()) {
    *result =
        protocol::Runtime::WebDriverValue::create()
            .setType(protocol::Runtime::WebDriverValue::TypeEnum::Function)
            .build();
    return Response::Success();
  }

  // Serialize as an Object.
  Response response =
      _serializeObject(value.As<v8::Object>(), context, max_depth, result);
  return response;
}

}  // namespace v8_inspector
