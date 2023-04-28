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

namespace {
using protocol::Response;
std::unique_ptr<protocol::Value> SerializeRecursively(
    v8::Local<v8::Value> value, v8::Local<v8::Context> context, int max_depth) {
  std::unique_ptr<ValueMirror> mirror = ValueMirror::create(context, value);
  std::unique_ptr<protocol::Runtime::WebDriverValue> web_driver_value =
      mirror->buildWebDriverValue(context, max_depth - 1);
  DCHECK(web_driver_value);

  std::unique_ptr<protocol::DictionaryValue> result_dict =
      protocol::DictionaryValue::create();

  result_dict->setValue(
      protocol::String("type"),
      protocol::StringValue::create(web_driver_value->getType()));
  if (web_driver_value->hasValue()) {
    result_dict->setValue(protocol::String("value"),
                          web_driver_value->getValue(nullptr)->clone());
  }
  return result_dict;
}

std::unique_ptr<protocol::Value> DescriptionForDate(
    v8::Local<v8::Context> context, v8::Local<v8::Date> date) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::String> dateISOString = date->ToISOString();
  return protocol::StringValue::create(
      toProtocolString(isolate, dateISOString));
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
  if (flags & v8::RegExp::Flags::kUnicodeSets) {
    result_string_builder.append('v');
  }
  if (flags & v8::RegExp::Flags::kSticky) result_string_builder.append('y');
  return result_string_builder.toString();
}

std::unique_ptr<protocol::Runtime::WebDriverValue> SerializeRegexp(
    v8::Local<v8::RegExp> value, v8::Local<v8::Context> context) {
  std::unique_ptr<protocol::Runtime::WebDriverValue> result =
      protocol::Runtime::WebDriverValue::create()
          .setType(protocol::Runtime::WebDriverValue::TypeEnum::Regexp)
          .build();

  std::unique_ptr<protocol::DictionaryValue> result_value =
      protocol::DictionaryValue::create();

  result_value->setValue(protocol::String("pattern"),
                         protocol::StringValue::create(toProtocolString(
                             context->GetIsolate(), value->GetSource())));

  String16 flags = _descriptionForRegExpFlags(value);
  if (!flags.isEmpty()) {
    result_value->setValue(protocol::String("flags"),
                           protocol::StringValue::create(flags));
  }
  result->setValue(std::move(result_value));
  return result;
}

std::unique_ptr<protocol::Runtime::WebDriverValue> SerializeDate(
    v8::Local<v8::Date> value, v8::Local<v8::Context> context) {
  std::unique_ptr<protocol::Runtime::WebDriverValue> result =
      protocol::Runtime::WebDriverValue::create()
          .setType(protocol::Runtime::WebDriverValue::TypeEnum::Date)
          .build();

  std::unique_ptr<protocol::Value> date_description =
      DescriptionForDate(context, value.As<v8::Date>());

  result->setValue(std::move(date_description));
  return result;
}

std::unique_ptr<protocol::Value> SerializeArrayValue(
    v8::Local<v8::Array> value, v8::Local<v8::Context> context, int max_depth) {
  std::unique_ptr<protocol::ListValue> result = protocol::ListValue::create();
  uint32_t length = value->Length();
  result->reserve(length);
  for (uint32_t i = 0; i < length; i++) {
    v8::Local<v8::Value> element_value;
    bool success = value->Get(context, i).ToLocal(&element_value);
    DCHECK(success);
    USE(success);

    std::unique_ptr<protocol::Value> element_protocol_value =
        SerializeRecursively(element_value, context, max_depth);

    result->pushValue(std::move(element_protocol_value));
  }
  return result;
}

std::unique_ptr<protocol::Runtime::WebDriverValue> SerializeArray(
    v8::Local<v8::Array> value, v8::Local<v8::Context> context, int max_depth) {
  std::unique_ptr<protocol::Runtime::WebDriverValue> result =
      protocol::Runtime::WebDriverValue::create()
          .setType(protocol::Runtime::WebDriverValue::TypeEnum::Array)
          .build();
  if (max_depth > 0) {
    result->setValue(SerializeArrayValue(value, context, max_depth));
  }

  return result;
}

std::unique_ptr<protocol::Runtime::WebDriverValue> SerializeMap(
    v8::Local<v8::Map> value, v8::Local<v8::Context> context, int max_depth) {
  std::unique_ptr<protocol::Runtime::WebDriverValue> result =
      protocol::Runtime::WebDriverValue::create()
          .setType(protocol::Runtime::WebDriverValue::TypeEnum::Map)
          .build();

  if (max_depth > 0) {
    std::unique_ptr<protocol::ListValue> result_value =
        protocol::ListValue::create();

    v8::Local<v8::Array> properties_and_values = value->AsArray();

    uint32_t length = properties_and_values->Length();
    result_value->reserve(length);
    for (uint32_t i = 0; i < length; i += 2) {
      v8::Local<v8::Value> key_value, property_value;
      std::unique_ptr<protocol::Value> key_protocol_value,
          property_protocol_value;

      bool success = properties_and_values->Get(context, i).ToLocal(&key_value);
      DCHECK(success);
      success =
          properties_and_values->Get(context, i + 1).ToLocal(&property_value);
      DCHECK(success);
      USE(success);

      if (key_value->IsString()) {
        key_protocol_value = protocol::StringValue::create(toProtocolString(
            context->GetIsolate(), key_value.As<v8::String>()));
      } else {
        key_protocol_value =
            SerializeRecursively(key_value, context, max_depth);
      }

      property_protocol_value =
          SerializeRecursively(property_value, context, max_depth);

      std::unique_ptr<protocol::ListValue> key_value_list =
          protocol::ListValue::create();

      key_value_list->pushValue(std::move(key_protocol_value));
      key_value_list->pushValue(std::move(property_protocol_value));

      result_value->pushValue(std::move(key_value_list));
    }
    result->setValue(std::move(result_value));
  }

  return result;
}

std::unique_ptr<protocol::Runtime::WebDriverValue> SerializeSet(
    v8::Local<v8::Set> value, v8::Local<v8::Context> context, int max_depth) {
  std::unique_ptr<protocol::Runtime::WebDriverValue> result =
      protocol::Runtime::WebDriverValue::create()
          .setType(protocol::Runtime::WebDriverValue::TypeEnum::Set)
          .build();

  if (max_depth > 0) {
    result->setValue(SerializeArrayValue(value->AsArray(), context, max_depth));
  }
  return result;
}

std::unique_ptr<protocol::Value> SerializeObjectValue(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context,
    int max_depth) {
  std::unique_ptr<protocol::ListValue> result = protocol::ListValue::create();
  // Iterate through object's properties.
  v8::Local<v8::Array> property_names;
  bool success = value->GetOwnPropertyNames(context).ToLocal(&property_names);
  DCHECK(success);

  uint32_t length = property_names->Length();
  result->reserve(length);
  for (uint32_t i = 0; i < length; i++) {
    v8::Local<v8::Value> key_value, property_value;
    std::unique_ptr<protocol::Value> key_protocol_value,
        property_protocol_value;

    success = property_names->Get(context, i).ToLocal(&key_value);
    DCHECK(success);

    if (key_value->IsString()) {
      v8::Maybe<bool> hasRealNamedProperty =
          value->HasRealNamedProperty(context, key_value.As<v8::String>());
      // Don't access properties with interceptors.
      if (hasRealNamedProperty.IsNothing() ||
          !hasRealNamedProperty.FromJust()) {
        continue;
      }
      key_protocol_value = protocol::StringValue::create(
          toProtocolString(context->GetIsolate(), key_value.As<v8::String>()));
    } else {
      key_protocol_value = SerializeRecursively(key_value, context, max_depth);
    }

    success = value->Get(context, key_value).ToLocal(&property_value);
    DCHECK(success);
    USE(success);

    property_protocol_value =
        SerializeRecursively(property_value, context, max_depth);

    std::unique_ptr<protocol::ListValue> key_value_list =
        protocol::ListValue::create();

    key_value_list->pushValue(std::move(key_protocol_value));
    key_value_list->pushValue(std::move(property_protocol_value));

    result->pushValue(std::move(key_value_list));
  }

  return result;
}

std::unique_ptr<protocol::Runtime::WebDriverValue> SerializeObject(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context,
    int max_depth) {
  std::unique_ptr<protocol::Runtime::WebDriverValue> result =
      protocol::Runtime::WebDriverValue::create()
          .setType(protocol::Runtime::WebDriverValue::TypeEnum::Object)
          .build();

  if (max_depth > 0) {
    result->setValue(
        SerializeObjectValue(value.As<v8::Object>(), context, max_depth));
  }
  return result;
}
}  // namespace

std::unique_ptr<protocol::Runtime::WebDriverValue>
V8WebDriverSerializer::serializeV8Value(v8::Local<v8::Object> value,
                                        v8::Local<v8::Context> context,
                                        int max_depth) {
  if (value->IsArray()) {
    return SerializeArray(value.As<v8::Array>(), context, max_depth);
  }
  if (value->IsRegExp()) {
    return SerializeRegexp(value.As<v8::RegExp>(), context);
  }
  if (value->IsDate()) {
    return SerializeDate(value.As<v8::Date>(), context);
  }
  if (value->IsMap()) {
    return SerializeMap(value.As<v8::Map>(), context, max_depth);
  }
  if (value->IsSet()) {
    return SerializeSet(value.As<v8::Set>(), context, max_depth);
  }
  if (value->IsWeakMap()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Weakmap)
        .build();
  }
  if (value->IsWeakSet()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Weakset)
        .build();
  }
  if (value->IsNativeError()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Error)
        .build();
  }
  if (value->IsProxy()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Proxy)
        .build();
  }
  if (value->IsPromise()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Promise)
        .build();
  }
  if (value->IsTypedArray()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Typedarray)
        .build();
  }
  if (value->IsArrayBuffer()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Arraybuffer)
        .build();
  }
  if (value->IsFunction()) {
    return protocol::Runtime::WebDriverValue::create()
        .setType(protocol::Runtime::WebDriverValue::TypeEnum::Function)
        .build();
  }

  // Serialize as an Object.
  return SerializeObject(value.As<v8::Object>(), context, max_depth);
}

}  // namespace v8_inspector
