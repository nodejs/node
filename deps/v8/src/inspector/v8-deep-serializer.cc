// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-deep-serializer.h"

#include "include/v8-container.h"
#include "include/v8-context.h"
#include "include/v8-date.h"
#include "include/v8-exception.h"
#include "include/v8-regexp.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-serialization-duplicate-tracker.h"
#include "src/inspector/value-mirror.h"

namespace v8_inspector {

namespace {
using protocol::Response;
std::unique_ptr<protocol::Value> SerializeRecursively(
    v8::Local<v8::Value> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker) {
  std::unique_ptr<ValueMirror> mirror = ValueMirror::create(context, value);
  return mirror->buildDeepSerializedValue(context, maxDepth - 1,
                                          duplicateTracker);
}

std::unique_ptr<protocol::Value> DescriptionForDate(
    v8::Local<v8::Context> context, v8::Local<v8::Date> date) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::String> dateISOString = date->ToISOString();
  return protocol::StringValue::create(
      toProtocolString(isolate, dateISOString));
}

String16 DescriptionForRegExpFlags(v8::Local<v8::RegExp> value) {
  String16Builder resultStringBuilder;
  v8::RegExp::Flags flags = value->GetFlags();
  if (flags & v8::RegExp::Flags::kHasIndices) resultStringBuilder.append('d');
  if (flags & v8::RegExp::Flags::kGlobal) resultStringBuilder.append('g');
  if (flags & v8::RegExp::Flags::kIgnoreCase) resultStringBuilder.append('i');
  if (flags & v8::RegExp::Flags::kLinear) resultStringBuilder.append('l');
  if (flags & v8::RegExp::Flags::kMultiline) resultStringBuilder.append('m');
  if (flags & v8::RegExp::Flags::kDotAll) resultStringBuilder.append('s');
  if (flags & v8::RegExp::Flags::kUnicode) resultStringBuilder.append('u');
  if (flags & v8::RegExp::Flags::kUnicodeSets) {
    resultStringBuilder.append('v');
  }
  if (flags & v8::RegExp::Flags::kSticky) resultStringBuilder.append('y');
  return resultStringBuilder.toString();
}

std::unique_ptr<protocol::DictionaryValue> SerializeRegexp(
    v8::Local<v8::RegExp> value, v8::Local<v8::Context> context,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  result->setString("type",
                    protocol::Runtime::DeepSerializedValue::TypeEnum::Regexp);

  std::unique_ptr<protocol::DictionaryValue> resultValue =
      protocol::DictionaryValue::create();

  resultValue->setValue(protocol::String("pattern"),
                        protocol::StringValue::create(toProtocolString(
                            context->GetIsolate(), value->GetSource())));

  String16 flags = DescriptionForRegExpFlags(value);
  if (!flags.isEmpty()) {
    resultValue->setValue(protocol::String("flags"),
                          protocol::StringValue::create(flags));
  }

  result->setValue("value", std::move(resultValue));
  return result;
}

std::unique_ptr<protocol::DictionaryValue> SerializeDate(
    v8::Local<v8::Date> value, v8::Local<v8::Context> context,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  result->setString("type",
                    protocol::Runtime::DeepSerializedValue::TypeEnum::Date);
  std::unique_ptr<protocol::Value> dateDescription =
      DescriptionForDate(context, value.As<v8::Date>());

  result->setValue("value", std::move(dateDescription));
  return result;
}

std::unique_ptr<protocol::Value> SerializeArrayValue(
    v8::Local<v8::Array> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker) {
  std::unique_ptr<protocol::ListValue> result = protocol::ListValue::create();
  uint32_t length = value->Length();
  result->reserve(length);
  for (uint32_t i = 0; i < length; i++) {
    v8::Local<v8::Value> elementValue;
    bool success = value->Get(context, i).ToLocal(&elementValue);
    DCHECK(success);
    USE(success);

    std::unique_ptr<protocol::Value> elementProtocolValue =
        SerializeRecursively(elementValue, context, maxDepth, duplicateTracker);

    result->pushValue(std::move(elementProtocolValue));
  }
  return result;
}

std::unique_ptr<protocol::DictionaryValue> SerializeArray(
    v8::Local<v8::Array> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  result->setString("type",
                    protocol::Runtime::DeepSerializedValue::TypeEnum::Array);

  if (maxDepth > 0) {
    result->setValue("value", SerializeArrayValue(value, context, maxDepth,
                                                  duplicateTracker));
  }

  return result;
}

std::unique_ptr<protocol::DictionaryValue> SerializeMap(
    v8::Local<v8::Map> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  result->setString("type",
                    protocol::Runtime::DeepSerializedValue::TypeEnum::Map);

  if (maxDepth > 0) {
    std::unique_ptr<protocol::ListValue> resultValue =
        protocol::ListValue::create();

    v8::Local<v8::Array> propertiesAndValues = value->AsArray();

    uint32_t length = propertiesAndValues->Length();
    resultValue->reserve(length);
    for (uint32_t i = 0; i < length; i += 2) {
      v8::Local<v8::Value> keyValue, propertyValue;
      std::unique_ptr<protocol::Value> keyProtocolValue, propertyProtocolValue;

      bool success = propertiesAndValues->Get(context, i).ToLocal(&keyValue);
      DCHECK(success);
      success =
          propertiesAndValues->Get(context, i + 1).ToLocal(&propertyValue);
      DCHECK(success);
      USE(success);

      if (keyValue->IsString()) {
        keyProtocolValue = protocol::StringValue::create(
            toProtocolString(context->GetIsolate(), keyValue.As<v8::String>()));
      } else {
        keyProtocolValue =
            SerializeRecursively(keyValue, context, maxDepth, duplicateTracker);
      }

      propertyProtocolValue = SerializeRecursively(propertyValue, context,
                                                   maxDepth, duplicateTracker);

      std::unique_ptr<protocol::ListValue> keyValueList =
          protocol::ListValue::create();

      keyValueList->pushValue(std::move(keyProtocolValue));
      keyValueList->pushValue(std::move(propertyProtocolValue));

      resultValue->pushValue(std::move(keyValueList));
    }
    result->setValue("value", std::move(resultValue));
  }

  return result;
}

std::unique_ptr<protocol::DictionaryValue> SerializeSet(
    v8::Local<v8::Set> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  result->setString("type",
                    protocol::Runtime::DeepSerializedValue::TypeEnum::Set);

  if (maxDepth > 0) {
    result->setValue("value", SerializeArrayValue(value->AsArray(), context,
                                                  maxDepth, duplicateTracker));
  }
  return result;
}

std::unique_ptr<protocol::Value> SerializeObjectValue(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker) {
  std::unique_ptr<protocol::ListValue> result = protocol::ListValue::create();
  // Iterate through object's properties.
  v8::Local<v8::Array> propertyNames;
  bool success = value->GetOwnPropertyNames(context).ToLocal(&propertyNames);
  DCHECK(success);

  uint32_t length = propertyNames->Length();
  result->reserve(length);
  for (uint32_t i = 0; i < length; i++) {
    v8::Local<v8::Value> keyValue, propertyValue;
    std::unique_ptr<protocol::Value> keyProtocolValue, propertyProtocolValue;

    success = propertyNames->Get(context, i).ToLocal(&keyValue);
    DCHECK(success);

    if (keyValue->IsString()) {
      v8::Maybe<bool> hasRealNamedProperty =
          value->HasRealNamedProperty(context, keyValue.As<v8::String>());
      // Don't access properties with interceptors.
      if (hasRealNamedProperty.IsNothing() ||
          !hasRealNamedProperty.FromJust()) {
        continue;
      }
      keyProtocolValue = protocol::StringValue::create(
          toProtocolString(context->GetIsolate(), keyValue.As<v8::String>()));
    } else {
      keyProtocolValue =
          SerializeRecursively(keyValue, context, maxDepth, duplicateTracker);
    }

    success = value->Get(context, keyValue).ToLocal(&propertyValue);
    DCHECK(success);
    USE(success);

    propertyProtocolValue = SerializeRecursively(propertyValue, context,
                                                 maxDepth, duplicateTracker);

    std::unique_ptr<protocol::ListValue> keyValueList =
        protocol::ListValue::create();

    keyValueList->pushValue(std::move(keyProtocolValue));
    keyValueList->pushValue(std::move(propertyProtocolValue));

    result->pushValue(std::move(keyValueList));
  }

  return result;
}

std::unique_ptr<protocol::DictionaryValue> SerializeObject(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  result->setString("type",
                    protocol::Runtime::DeepSerializedValue::TypeEnum::Object);

  if (maxDepth > 0) {
    result->setValue(
        "value", SerializeObjectValue(value.As<v8::Object>(), context, maxDepth,
                                      duplicateTracker));
  }
  return result;
}
}  // namespace

std::unique_ptr<protocol::DictionaryValue> V8DeepSerializer::serializeV8Value(
    v8::Local<v8::Object> value, v8::Local<v8::Context> context, int maxDepth,
    V8SerializationDuplicateTracker& duplicateTracker,
    std::unique_ptr<protocol::DictionaryValue> result) {
  if (value->IsArray()) {
    return SerializeArray(value.As<v8::Array>(), context, maxDepth,
                          duplicateTracker, std::move(result));
  }
  if (value->IsRegExp()) {
    return SerializeRegexp(value.As<v8::RegExp>(), context, duplicateTracker,
                           std::move(result));
  }
  if (value->IsDate()) {
    return SerializeDate(value.As<v8::Date>(), context, duplicateTracker,
                         std::move(result));
  }
  if (value->IsMap()) {
    return SerializeMap(value.As<v8::Map>(), context, maxDepth,
                        duplicateTracker, std::move(result));
  }
  if (value->IsSet()) {
    return SerializeSet(value.As<v8::Set>(), context, maxDepth,
                        duplicateTracker, std::move(result));
  }
  if (value->IsWeakMap()) {
    result->setString(
        "type", protocol::Runtime::DeepSerializedValue::TypeEnum::Weakmap);
    return result;
  }
  if (value->IsWeakSet()) {
    result->setString(
        "type", protocol::Runtime::DeepSerializedValue::TypeEnum::Weakset);
    return result;
  }
  if (value->IsNativeError()) {
    result->setString("type",
                      protocol::Runtime::DeepSerializedValue::TypeEnum::Error);
    return result;
  }
  if (value->IsProxy()) {
    result->setString("type",
                      protocol::Runtime::DeepSerializedValue::TypeEnum::Proxy);
    return result;
  }
  if (value->IsPromise()) {
    result->setString(
        "type", protocol::Runtime::DeepSerializedValue::TypeEnum::Promise);
    return result;
  }
  if (value->IsTypedArray()) {
    result->setString(
        "type", protocol::Runtime::DeepSerializedValue::TypeEnum::Typedarray);
    return result;
  }
  if (value->IsArrayBuffer()) {
    result->setString(
        "type", protocol::Runtime::DeepSerializedValue::TypeEnum::Arraybuffer);
    return result;
  }
  if (value->IsFunction()) {
    result->setString(
        "type", protocol::Runtime::DeepSerializedValue::TypeEnum::Function);
    return result;
  }

  // Serialize as an Object.
  return SerializeObject(value.As<v8::Object>(), context, maxDepth,
                         duplicateTracker, std::move(result));
}

}  // namespace v8_inspector
