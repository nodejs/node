// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/value-mirror.h"

#include <algorithm>
#include <cmath>

#include "src/base/optional.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-value-utils.h"

namespace v8_inspector {

using protocol::Response;
using protocol::Runtime::EntryPreview;
using protocol::Runtime::ObjectPreview;
using protocol::Runtime::PropertyPreview;
using protocol::Runtime::RemoteObject;

namespace {
V8InspectorClient* clientFor(v8::Local<v8::Context> context) {
  return static_cast<V8InspectorImpl*>(
             v8::debug::GetInspector(context->GetIsolate()))
      ->client();
}

V8InternalValueType v8InternalValueTypeFrom(v8::Local<v8::Context> context,
                                            v8::Local<v8::Value> value) {
  if (!value->IsObject()) return V8InternalValueType::kNone;
  V8InspectorImpl* inspector = static_cast<V8InspectorImpl*>(
      v8::debug::GetInspector(context->GetIsolate()));
  int contextId = InspectedContext::contextId(context);
  InspectedContext* inspectedContext = inspector->getContext(contextId);
  if (!inspectedContext) return V8InternalValueType::kNone;
  return inspectedContext->getInternalType(value.As<v8::Object>());
}

template <typename ResultType>
ResultType unpackWasmValue(v8::Local<v8::Context> context,
                           v8::Local<v8::Array> array) {
  ResultType result;
  constexpr int kSize = sizeof(result);
  uint8_t buffer[kSize];
  for (int i = 0; i < kSize; i++) {
    v8::Local<v8::Int32> i32 =
        array->Get(context, i).ToLocalChecked().As<v8::Int32>();
    buffer[i] = static_cast<uint8_t>(i32->Value());
  }
  memcpy(&result, buffer, kSize);
  return result;
}

String16 descriptionForWasmS128(std::array<uint8_t, 16> arr) {
  String16Builder builder;
  for (int i = 0; i < 16; i++) {
    builder.appendUnsignedAsHex(arr.at(i));
    builder.append(" ");
  }
  return builder.toString();
}

// Partial list of Wasm's ValueType, copied here to avoid including internal
// header. Using an unscoped enumeration here to allow implicit conversions from
// int. Keep in sync with ValueType::Kind in wasm/value-type.h.
enum WasmValueType { kStmt, kI32, kI64, kF32, kF64, kS128, kExternRef };

Response toProtocolValue(v8::Local<v8::Context> context,
                         v8::Local<v8::Value> value, int maxDepth,
                         std::unique_ptr<protocol::Value>* result) {
  if (!maxDepth)
    return Response::ServerError("Object reference chain is too long");
  maxDepth--;

  if (value->IsNull() || value->IsUndefined()) {
    *result = protocol::Value::null();
    return Response::Success();
  }
  if (value->IsBoolean()) {
    *result =
        protocol::FundamentalValue::create(value.As<v8::Boolean>()->Value());
    return Response::Success();
  }
  if (value->IsNumber()) {
    double doubleValue = value.As<v8::Number>()->Value();
    if (doubleValue >= std::numeric_limits<int>::min() &&
        doubleValue <= std::numeric_limits<int>::max() &&
        bit_cast<int64_t>(doubleValue) != bit_cast<int64_t>(-0.0)) {
      int intValue = static_cast<int>(doubleValue);
      if (intValue == doubleValue) {
        *result = protocol::FundamentalValue::create(intValue);
        return Response::Success();
      }
    }
    *result = protocol::FundamentalValue::create(doubleValue);
    return Response::Success();
  }
  if (value->IsString()) {
    *result = protocol::StringValue::create(
        toProtocolString(context->GetIsolate(), value.As<v8::String>()));
    return Response::Success();
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
      if (!response.IsSuccess()) return response;
      inspectorArray->pushValue(std::move(element));
    }
    *result = std::move(inspectorArray);
    return Response::Success();
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
      if (!response.IsSuccess()) return response;
      jsonObject->setValue(
          toProtocolString(context->GetIsolate(), propertyName),
          std::move(propertyValue));
    }
    *result = std::move(jsonObject);
    return Response::Success();
  }

  if (v8::debug::WasmValue::IsWasmValue(value)) {
    auto wasmValue = value.As<v8::debug::WasmValue>();

    // Convert serializable Wasm values (i32, f32, f64) into protocol values.
    // Not all i64 values are representable by double, so always represent it as
    // a String here.
    switch (wasmValue->value_type()) {
      case kI32: {
        *result = protocol::FundamentalValue::create(
            unpackWasmValue<int32_t>(context, wasmValue->bytes()));
        break;
      }
      case kI64: {
        *result = protocol::StringValue::create(String16::fromInteger64(
            unpackWasmValue<int64_t>(context, wasmValue->bytes())));
        break;
      }
      case kF32: {
        *result = protocol::FundamentalValue::create(
            unpackWasmValue<float>(context, wasmValue->bytes()));
        break;
      }
      case kF64: {
        *result = protocol::FundamentalValue::create(
            unpackWasmValue<double>(context, wasmValue->bytes()));
        break;
      }
      case kS128: {
        auto bytes = wasmValue->bytes();
        DCHECK_EQ(16, bytes->Length());
        auto s128 = unpackWasmValue<std::array<uint8_t, 16>>(context, bytes);
        String16 desc = descriptionForWasmS128(s128);
        *result = protocol::StringValue::create(desc);
        break;
      }
      case kExternRef: {
        std::unique_ptr<protocol::Value> externrefValue;
        Response response = toProtocolValue(context, wasmValue->ref(), maxDepth,
                                            &externrefValue);
        if (!response.IsSuccess()) return response;
        *result = std::move(externrefValue);
        break;
      }
      default: {
        UNIMPLEMENTED();
      }
    }
    return Response::Success();
  }

  return Response::ServerError("Object couldn't be returned by value");
}

Response toProtocolValue(v8::Local<v8::Context> context,
                         v8::Local<v8::Value> value,
                         std::unique_ptr<protocol::Value>* result) {
  if (value->IsUndefined()) return Response::Success();
  return toProtocolValue(context, value, 1000, result);
}

enum AbbreviateMode { kMiddle, kEnd };

String16 abbreviateString(const String16& value, AbbreviateMode mode) {
  const size_t maxLength = 100;
  if (value.length() <= maxLength) return value;
  UChar ellipsis = static_cast<UChar>(0x2026);
  if (mode == kMiddle) {
    return String16::concat(
        value.substring(0, maxLength / 2), String16(&ellipsis, 1),
        value.substring(value.length() - maxLength / 2 + 1));
  }
  return String16::concat(value.substring(0, maxLength - 1), ellipsis);
}

String16 descriptionForSymbol(v8::Local<v8::Context> context,
                              v8::Local<v8::Symbol> symbol) {
  return String16::concat("Symbol(",
                          toProtocolStringWithTypeCheck(context->GetIsolate(),
                                                        symbol->Description()),
                          ")");
}

String16 descriptionForBigInt(v8::Local<v8::Context> context,
                              v8::Local<v8::BigInt> value) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::String> description;
  if (!value->ToString(context).ToLocal(&description)) return String16();
  return toProtocolString(isolate, description) + "n";
}

String16 descriptionForPrimitiveType(v8::Local<v8::Context> context,
                                     v8::Local<v8::Value> value) {
  if (value->IsUndefined()) return RemoteObject::TypeEnum::Undefined;
  if (value->IsNull()) return RemoteObject::SubtypeEnum::Null;
  if (value->IsBoolean()) {
    return value.As<v8::Boolean>()->Value() ? "true" : "false";
  }
  if (value->IsString()) {
    return toProtocolString(context->GetIsolate(), value.As<v8::String>());
  }
  UNREACHABLE();
  return String16();
}

String16 descriptionForRegExp(v8::Isolate* isolate,
                              v8::Local<v8::RegExp> value) {
  String16Builder description;
  description.append('/');
  description.append(toProtocolString(isolate, value->GetSource()));
  description.append('/');
  v8::RegExp::Flags flags = value->GetFlags();
  if (flags & v8::RegExp::Flags::kGlobal) description.append('g');
  if (flags & v8::RegExp::Flags::kIgnoreCase) description.append('i');
  if (flags & v8::RegExp::Flags::kMultiline) description.append('m');
  if (flags & v8::RegExp::Flags::kDotAll) description.append('s');
  if (flags & v8::RegExp::Flags::kUnicode) description.append('u');
  if (flags & v8::RegExp::Flags::kSticky) description.append('y');
  return description.toString();
}

enum class ErrorType { kNative, kClient };

// Build a description from an exception using the following rules:
//   * Usually return the stack trace found in the {stack} property.
//   * If the stack trace does not start with the class name of the passed
//     exception, try to build a description from the class name, the
//     {message} property and the rest of the stack trace.
//     (The stack trace is only used if {message} was also found in
//     said stack trace).
String16 descriptionForError(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> object, ErrorType type) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  String16 className = toProtocolString(isolate, object->GetConstructorName());

  v8::base::Optional<String16> stack;
  {
    v8::Local<v8::Value> stackValue;
    if (object->Get(context, toV8String(isolate, "stack"))
            .ToLocal(&stackValue) &&
        stackValue->IsString()) {
      stack = toProtocolString(isolate, stackValue.As<v8::String>());
    }
  }

  if (type == ErrorType::kNative && stack) return *stack;

  if (stack && stack->substring(0, className.length()) == className) {
    return *stack;
  }

  v8::base::Optional<String16> message;
  {
    v8::Local<v8::Value> messageValue;
    if (object->Get(context, toV8String(isolate, "message"))
            .ToLocal(&messageValue) &&
        messageValue->IsString()) {
      String16 msg = toProtocolStringWithTypeCheck(isolate, messageValue);
      if (!msg.isEmpty()) message = msg;
    }
  }

  if (!message) return stack ? *stack : className;

  String16 description = className + ": " + *message;
  if (!stack) return description;

  DCHECK(stack && message);
  size_t index = stack->find(*message);
  String16 stackWithoutMessage =
      index != String16::kNotFound ? stack->substring(index + message->length())
                                   : String16();
  return description + stackWithoutMessage;
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

String16 descriptionForScopeList(v8::Local<v8::Array> list) {
  return String16::concat(
      "Scopes[", String16::fromInteger(static_cast<size_t>(list->Length())),
      ']');
}

String16 descriptionForScope(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> object) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Value> value;
  if (!object->GetRealNamedProperty(context, toV8String(isolate, "description"))
           .ToLocal(&value)) {
    return String16();
  }
  return toProtocolStringWithTypeCheck(isolate, value);
}

String16 descriptionForCollection(v8::Isolate* isolate,
                                  v8::Local<v8::Object> object, size_t length) {
  String16 className = toProtocolString(isolate, object->GetConstructorName());
  return String16::concat(className, '(', String16::fromInteger(length), ')');
}

String16 descriptionForEntry(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> object) {
  v8::Isolate* isolate = context->GetIsolate();
  String16 key;
  v8::Local<v8::Value> tmp;
  if (object->GetRealNamedProperty(context, toV8String(isolate, "key"))
          .ToLocal(&tmp)) {
    auto wrapper = ValueMirror::create(context, tmp);
    if (wrapper) {
      std::unique_ptr<ObjectPreview> preview;
      int limit = 5;
      wrapper->buildEntryPreview(context, &limit, &limit, &preview);
      if (preview) {
        key = preview->getDescription(String16());
        if (preview->getType() == RemoteObject::TypeEnum::String) {
          key = String16::concat('\"', key, '\"');
        }
      }
    }
  }

  String16 value;
  if (object->GetRealNamedProperty(context, toV8String(isolate, "value"))
          .ToLocal(&tmp)) {
    auto wrapper = ValueMirror::create(context, tmp);
    if (wrapper) {
      std::unique_ptr<ObjectPreview> preview;
      int limit = 5;
      wrapper->buildEntryPreview(context, &limit, &limit, &preview);
      if (preview) {
        value = preview->getDescription(String16());
        if (preview->getType() == RemoteObject::TypeEnum::String) {
          value = String16::concat('\"', value, '\"');
        }
      }
    }
  }

  return key.length() ? ("{" + key + " => " + value + "}") : value;
}

String16 descriptionForFunction(v8::Local<v8::Context> context,
                                v8::Local<v8::Function> value) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::String> description;
  if (!value->ToString(context).ToLocal(&description)) {
    return descriptionForObject(isolate, value);
  }
  return toProtocolString(isolate, description);
}

class PrimitiveValueMirror final : public ValueMirror {
 public:
  PrimitiveValueMirror(v8::Local<v8::Value> value, const String16& type)
      : m_value(value), m_type(type) {}

  v8::Local<v8::Value> v8Value() const override { return m_value; }
  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    std::unique_ptr<protocol::Value> protocolValue;
    toProtocolValue(context, m_value, &protocolValue);
    *result = RemoteObject::create()
                  .setType(m_type)
                  .setValue(std::move(protocolValue))
                  .build();
    if (m_value->IsNull())
      (*result)->setSubtype(RemoteObject::SubtypeEnum::Null);
    return Response::Success();
  }

  void buildEntryPreview(
      v8::Local<v8::Context> context, int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* preview) const override {
    *preview =
        ObjectPreview::create()
            .setType(m_type)
            .setDescription(descriptionForPrimitiveType(context, m_value))
            .setOverflow(false)
            .setProperties(std::make_unique<protocol::Array<PropertyPreview>>())
            .build();
    if (m_value->IsNull())
      (*preview)->setSubtype(RemoteObject::SubtypeEnum::Null);
  }

  void buildPropertyPreview(
      v8::Local<v8::Context> context, const String16& name,
      std::unique_ptr<PropertyPreview>* preview) const override {
    *preview = PropertyPreview::create()
                   .setName(name)
                   .setValue(abbreviateString(
                       descriptionForPrimitiveType(context, m_value), kMiddle))
                   .setType(m_type)
                   .build();
    if (m_value->IsNull())
      (*preview)->setSubtype(RemoteObject::SubtypeEnum::Null);
  }

 private:
  v8::Local<v8::Value> m_value;
  String16 m_type;
  String16 m_subtype;
};

class WasmValueMirror final : public ValueMirror {
 public:
  explicit WasmValueMirror(v8::Local<v8::debug::WasmValue> value)
      : m_value(value) {}

  v8::Local<v8::Value> v8Value() const override { return m_value; }

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    bool serializable;
    String16 descriptionValue = description(context, &serializable);
    *result = RemoteObject::create()
                  .setType(RemoteObject::TypeEnum::Wasm)
                  .setSubtype(subtype())
                  .setDescription(descriptionValue)
                  .build();
    if (serializable) {
      std::unique_ptr<protocol::Value> protocolValue;
      toProtocolValue(context, m_value, &protocolValue);
      (*result)->setValue(std::move(protocolValue));
    } else {
      (*result)->setUnserializableValue(descriptionValue);
    }
    return Response::Success();
  }

  void buildPropertyPreview(
      v8::Local<v8::Context> context, const String16& name,
      std::unique_ptr<PropertyPreview>* result) const override {
    bool serializable;
    *result = PropertyPreview::create()
                  .setName(name)
                  .setType(RemoteObject::TypeEnum::Wasm)
                  .setSubtype(subtype())
                  .setValue(description(context, &serializable))
                  .build();
  }

  void buildEntryPreview(
      v8::Local<v8::Context> context, int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* preview) const override {
    bool serializable;
    *preview =
        ObjectPreview::create()
            .setType(RemoteObject::TypeEnum::Wasm)
            .setSubtype(subtype())
            .setDescription(description(context, &serializable))
            .setOverflow(false)
            .setProperties(std::make_unique<protocol::Array<PropertyPreview>>())
            .build();
  }

 private:
  String16 subtype() const {
    switch (m_value->value_type()) {
      case kI32:
        return RemoteObject::SubtypeEnum::I32;
      case kI64:
        return RemoteObject::SubtypeEnum::I64;
      case kF32:
        return RemoteObject::SubtypeEnum::F32;
      case kF64:
        return RemoteObject::SubtypeEnum::F64;
      case kS128:
        return RemoteObject::SubtypeEnum::V128;
      case kExternRef:
        return RemoteObject::SubtypeEnum::Externref;
      default:
        UNREACHABLE();
    }
  }

  String16 description(v8::Local<v8::Context> context,
                       bool* serializable) const {
    *serializable = true;
    switch (m_value->value_type()) {
      case kI32: {
        return String16::fromInteger(
            unpackWasmValue<int32_t>(context, m_value->bytes()));
      }
      case kI64: {
        *serializable = false;
        return String16::fromInteger64(
            unpackWasmValue<int64_t>(context, m_value->bytes()));
      }
      case kF32: {
        return String16::fromDouble(
            unpackWasmValue<float>(context, m_value->bytes()));
      }
      case kF64: {
        return String16::fromDouble(
            unpackWasmValue<double>(context, m_value->bytes()));
      }
      case kS128: {
        *serializable = false;
        auto bytes = m_value->bytes();
        DCHECK_EQ(16, bytes->Length());
        auto s128 = unpackWasmValue<std::array<uint8_t, 16>>(context, bytes);
        return descriptionForWasmS128(s128);
      }
      case kExternRef: {
        return descriptionForObject(context->GetIsolate(),
                                    m_value->ref().As<v8::Object>());
      }
      default: {
        *serializable = false;
        return String16("Unknown");
      }
    }
  }

  v8::Local<v8::debug::WasmValue> m_value;
};

class NumberMirror final : public ValueMirror {
 public:
  explicit NumberMirror(v8::Local<v8::Number> value) : m_value(value) {}
  v8::Local<v8::Value> v8Value() const override { return m_value; }

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    bool unserializable = false;
    String16 descriptionValue = description(&unserializable);
    *result = RemoteObject::create()
                  .setType(RemoteObject::TypeEnum::Number)
                  .setDescription(descriptionValue)
                  .build();
    if (unserializable) {
      (*result)->setUnserializableValue(descriptionValue);
    } else {
      (*result)->setValue(protocol::FundamentalValue::create(m_value->Value()));
    }
    return Response::Success();
  }
  void buildPropertyPreview(
      v8::Local<v8::Context> context, const String16& name,
      std::unique_ptr<PropertyPreview>* result) const override {
    bool unserializable = false;
    *result = PropertyPreview::create()
                  .setName(name)
                  .setType(RemoteObject::TypeEnum::Number)
                  .setValue(description(&unserializable))
                  .build();
  }
  void buildEntryPreview(
      v8::Local<v8::Context> context, int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* preview) const override {
    bool unserializable = false;
    *preview =
        ObjectPreview::create()
            .setType(RemoteObject::TypeEnum::Number)
            .setDescription(description(&unserializable))
            .setOverflow(false)
            .setProperties(std::make_unique<protocol::Array<PropertyPreview>>())
            .build();
  }

 private:
  String16 description(bool* unserializable) const {
    *unserializable = true;
    double rawValue = m_value->Value();
    if (std::isnan(rawValue)) return "NaN";
    if (rawValue == 0.0 && std::signbit(rawValue)) return "-0";
    if (std::isinf(rawValue)) {
      return std::signbit(rawValue) ? "-Infinity" : "Infinity";
    }
    *unserializable = false;
    return String16::fromDouble(rawValue);
  }

  v8::Local<v8::Number> m_value;
};

class BigIntMirror final : public ValueMirror {
 public:
  explicit BigIntMirror(v8::Local<v8::BigInt> value) : m_value(value) {}

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    String16 description = descriptionForBigInt(context, m_value);
    *result = RemoteObject::create()
                  .setType(RemoteObject::TypeEnum::Bigint)
                  .setUnserializableValue(description)
                  .setDescription(description)
                  .build();
    return Response::Success();
  }

  void buildPropertyPreview(v8::Local<v8::Context> context,
                            const String16& name,
                            std::unique_ptr<protocol::Runtime::PropertyPreview>*
                                preview) const override {
    *preview = PropertyPreview::create()
                   .setName(name)
                   .setType(RemoteObject::TypeEnum::Bigint)
                   .setValue(abbreviateString(
                       descriptionForBigInt(context, m_value), kMiddle))
                   .build();
  }

  void buildEntryPreview(v8::Local<v8::Context> context, int* nameLimit,
                         int* indexLimit,
                         std::unique_ptr<protocol::Runtime::ObjectPreview>*
                             preview) const override {
    *preview =
        ObjectPreview::create()
            .setType(RemoteObject::TypeEnum::Bigint)
            .setDescription(descriptionForBigInt(context, m_value))
            .setOverflow(false)
            .setProperties(std::make_unique<protocol::Array<PropertyPreview>>())
            .build();
  }

  v8::Local<v8::Value> v8Value() const override { return m_value; }

 private:
  v8::Local<v8::BigInt> m_value;
};

class SymbolMirror final : public ValueMirror {
 public:
  explicit SymbolMirror(v8::Local<v8::Value> value)
      : m_symbol(value.As<v8::Symbol>()) {}

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    if (mode == WrapMode::kForceValue) {
      return Response::ServerError("Object couldn't be returned by value");
    }
    *result = RemoteObject::create()
                  .setType(RemoteObject::TypeEnum::Symbol)
                  .setDescription(descriptionForSymbol(context, m_symbol))
                  .build();
    return Response::Success();
  }

  void buildPropertyPreview(v8::Local<v8::Context> context,
                            const String16& name,
                            std::unique_ptr<protocol::Runtime::PropertyPreview>*
                                preview) const override {
    *preview = PropertyPreview::create()
                   .setName(name)
                   .setType(RemoteObject::TypeEnum::Symbol)
                   .setValue(abbreviateString(
                       descriptionForSymbol(context, m_symbol), kEnd))
                   .build();
  }

  v8::Local<v8::Value> v8Value() const override { return m_symbol; }

 private:
  v8::Local<v8::Symbol> m_symbol;
};

class LocationMirror final : public ValueMirror {
 public:
  static std::unique_ptr<LocationMirror> create(
      v8::Local<v8::Function> function) {
    return create(function, function->ScriptId(),
                  function->GetScriptLineNumber(),
                  function->GetScriptColumnNumber());
  }
  static std::unique_ptr<LocationMirror> createForGenerator(
      v8::Local<v8::Value> value) {
    v8::Local<v8::debug::GeneratorObject> generatorObject =
        v8::debug::GeneratorObject::Cast(value);
    if (!generatorObject->IsSuspended()) {
      return create(generatorObject->Function());
    }
    v8::Local<v8::debug::Script> script;
    if (!generatorObject->Script().ToLocal(&script)) return nullptr;
    v8::debug::Location suspendedLocation =
        generatorObject->SuspendedLocation();
    return create(value, script->Id(), suspendedLocation.GetLineNumber(),
                  suspendedLocation.GetColumnNumber());
  }

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    auto location = protocol::DictionaryValue::create();
    location->setString("scriptId", String16::fromInteger(m_scriptId));
    location->setInteger("lineNumber", m_lineNumber);
    location->setInteger("columnNumber", m_columnNumber);
    *result = RemoteObject::create()
                  .setType(RemoteObject::TypeEnum::Object)
                  .setSubtype("internal#location")
                  .setDescription("Object")
                  .setValue(std::move(location))
                  .build();
    return Response::Success();
  }
  v8::Local<v8::Value> v8Value() const override { return m_value; }

 private:
  static std::unique_ptr<LocationMirror> create(v8::Local<v8::Value> value,
                                                int scriptId, int lineNumber,
                                                int columnNumber) {
    if (scriptId == v8::UnboundScript::kNoScriptId) return nullptr;
    if (lineNumber == v8::Function::kLineOffsetNotFound ||
        columnNumber == v8::Function::kLineOffsetNotFound) {
      return nullptr;
    }
    return std::unique_ptr<LocationMirror>(
        new LocationMirror(value, scriptId, lineNumber, columnNumber));
  }

  LocationMirror(v8::Local<v8::Value> value, int scriptId, int lineNumber,
                 int columnNumber)
      : m_value(value),
        m_scriptId(scriptId),
        m_lineNumber(lineNumber),
        m_columnNumber(columnNumber) {}

  v8::Local<v8::Value> m_value;
  int m_scriptId;
  int m_lineNumber;
  int m_columnNumber;
};

class FunctionMirror final : public ValueMirror {
 public:
  explicit FunctionMirror(v8::Local<v8::Value> value)
      : m_value(value.As<v8::Function>()) {}

  v8::Local<v8::Value> v8Value() const override { return m_value; }

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    // TODO(alph): drop this functionality.
    if (mode == WrapMode::kForceValue) {
      std::unique_ptr<protocol::Value> protocolValue;
      Response response = toProtocolValue(context, m_value, &protocolValue);
      if (!response.IsSuccess()) return response;
      *result = RemoteObject::create()
                    .setType(RemoteObject::TypeEnum::Function)
                    .setValue(std::move(protocolValue))
                    .build();
    } else {
      *result = RemoteObject::create()
                    .setType(RemoteObject::TypeEnum::Function)
                    .setClassName(toProtocolStringWithTypeCheck(
                        context->GetIsolate(), m_value->GetConstructorName()))
                    .setDescription(descriptionForFunction(context, m_value))
                    .build();
    }
    return Response::Success();
  }

  void buildPropertyPreview(
      v8::Local<v8::Context> context, const String16& name,
      std::unique_ptr<PropertyPreview>* result) const override {
    *result = PropertyPreview::create()
                  .setName(name)
                  .setType(RemoteObject::TypeEnum::Function)
                  .setValue(String16())
                  .build();
  }
  void buildEntryPreview(
      v8::Local<v8::Context> context, int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* preview) const override {
    *preview =
        ObjectPreview::create()
            .setType(RemoteObject::TypeEnum::Function)
            .setDescription(descriptionForFunction(context, m_value))
            .setOverflow(false)
            .setProperties(std::make_unique<protocol::Array<PropertyPreview>>())
            .build();
  }

 private:
  v8::Local<v8::Function> m_value;
};

bool isArrayLike(v8::Local<v8::Context> context, v8::Local<v8::Value> value,
                 size_t* length) {
  if (!value->IsObject()) return false;
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Object> object = value.As<v8::Object>();
  v8::Local<v8::Value> spliceValue;
  if (!object->IsArgumentsObject() &&
      (!object->GetRealNamedProperty(context, toV8String(isolate, "splice"))
            .ToLocal(&spliceValue) ||
       !spliceValue->IsFunction())) {
    return false;
  }
  v8::Local<v8::Value> lengthValue;
  v8::Maybe<bool> result =
      object->HasOwnProperty(context, toV8String(isolate, "length"));
  if (result.IsNothing()) return false;
  if (!result.FromJust() ||
      !object->Get(context, toV8String(isolate, "length"))
           .ToLocal(&lengthValue) ||
      !lengthValue->IsUint32()) {
    return false;
  }
  *length = v8::Local<v8::Uint32>::Cast(lengthValue)->Value();
  return true;
}

struct EntryMirror {
  std::unique_ptr<ValueMirror> key;
  std::unique_ptr<ValueMirror> value;

  static bool getEntries(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> object, size_t limit,
                         bool* overflow, std::vector<EntryMirror>* mirrors) {
    bool isKeyValue = false;
    v8::Local<v8::Array> entries;
    if (!object->PreviewEntries(&isKeyValue).ToLocal(&entries)) return false;
    for (uint32_t i = 0; i < entries->Length(); i += isKeyValue ? 2 : 1) {
      v8::Local<v8::Value> tmp;

      std::unique_ptr<ValueMirror> keyMirror;
      if (isKeyValue && entries->Get(context, i).ToLocal(&tmp)) {
        keyMirror = ValueMirror::create(context, tmp);
      }
      std::unique_ptr<ValueMirror> valueMirror;
      if (entries->Get(context, isKeyValue ? i + 1 : i).ToLocal(&tmp)) {
        valueMirror = ValueMirror::create(context, tmp);
      } else {
        continue;
      }
      if (mirrors->size() == limit) {
        *overflow = true;
        return true;
      }
      mirrors->emplace_back(
          EntryMirror{std::move(keyMirror), std::move(valueMirror)});
    }
    return mirrors->size() > 0;
  }
};

class PreviewPropertyAccumulator : public ValueMirror::PropertyAccumulator {
 public:
  PreviewPropertyAccumulator(const std::vector<String16>& blocklist,
                             int skipIndex, int* nameLimit, int* indexLimit,
                             bool* overflow,
                             std::vector<PropertyMirror>* mirrors)
      : m_blocklist(blocklist),
        m_skipIndex(skipIndex),
        m_nameLimit(nameLimit),
        m_indexLimit(indexLimit),
        m_overflow(overflow),
        m_mirrors(mirrors) {}

  bool Add(PropertyMirror mirror) override {
    if (mirror.exception) return true;
    if ((!mirror.getter || !mirror.getter->v8Value()->IsFunction()) &&
        !mirror.value) {
      return true;
    }
    if (!mirror.isOwn) return true;
    if (std::find(m_blocklist.begin(), m_blocklist.end(), mirror.name) !=
        m_blocklist.end()) {
      return true;
    }
    if (mirror.isIndex && m_skipIndex > 0) {
      --m_skipIndex;
      if (m_skipIndex > 0) return true;
    }
    int* limit = mirror.isIndex ? m_indexLimit : m_nameLimit;
    if (!*limit) {
      *m_overflow = true;
      return false;
    }
    --*limit;
    m_mirrors->push_back(std::move(mirror));
    return true;
  }

 private:
  std::vector<String16> m_blocklist;
  int m_skipIndex;
  int* m_nameLimit;
  int* m_indexLimit;
  bool* m_overflow;
  std::vector<PropertyMirror>* m_mirrors;
};

bool getPropertiesForPreview(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> object, int* nameLimit,
                             int* indexLimit, bool* overflow,
                             std::vector<PropertyMirror>* properties) {
  std::vector<String16> blocklist;
  size_t length = 0;
  if (object->IsArray() || isArrayLike(context, object, &length) ||
      object->IsStringObject()) {
    blocklist.push_back("length");
  } else {
    auto clientSubtype = clientFor(context)->valueSubtype(object);
    if (clientSubtype && toString16(clientSubtype->string()) == "array") {
      blocklist.push_back("length");
    }
  }
  if (object->IsArrayBuffer() || object->IsSharedArrayBuffer()) {
    blocklist.push_back("[[Int8Array]]");
    blocklist.push_back("[[Uint8Array]]");
    blocklist.push_back("[[Int16Array]]");
    blocklist.push_back("[[Int32Array]]");
  }
  int skipIndex = object->IsStringObject()
                      ? object.As<v8::StringObject>()->ValueOf()->Length() + 1
                      : -1;
  PreviewPropertyAccumulator accumulator(blocklist, skipIndex, nameLimit,
                                         indexLimit, overflow, properties);
  return ValueMirror::getProperties(context, object, false, false,
                                    &accumulator);
}

void getInternalPropertiesForPreview(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object,
    int* nameLimit, bool* overflow,
    std::vector<InternalPropertyMirror>* properties) {
  std::vector<InternalPropertyMirror> mirrors;
  ValueMirror::getInternalProperties(context, object, &mirrors);
  std::vector<String16> allowlist;
  if (object->IsBooleanObject() || object->IsNumberObject() ||
      object->IsStringObject() || object->IsSymbolObject() ||
      object->IsBigIntObject()) {
    allowlist.emplace_back("[[PrimitiveValue]]");
  } else if (object->IsPromise()) {
    allowlist.emplace_back("[[PromiseState]]");
    allowlist.emplace_back("[[PromiseResult]]");
  } else if (object->IsGeneratorObject()) {
    allowlist.emplace_back("[[GeneratorState]]");
  }
  for (auto& mirror : mirrors) {
    if (std::find(allowlist.begin(), allowlist.end(), mirror.name) ==
        allowlist.end()) {
      continue;
    }
    if (!*nameLimit) {
      *overflow = true;
      return;
    }
    --*nameLimit;
    properties->push_back(std::move(mirror));
  }
}

void getPrivatePropertiesForPreview(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object,
    int* nameLimit, bool* overflow,
    protocol::Array<PropertyPreview>* privateProperties) {
  std::vector<PrivatePropertyMirror> mirrors =
      ValueMirror::getPrivateProperties(context, object);
  for (auto& mirror : mirrors) {
    std::unique_ptr<PropertyPreview> propertyPreview;
    if (mirror.value) {
      mirror.value->buildPropertyPreview(context, mirror.name,
                                         &propertyPreview);
    } else {
      propertyPreview = PropertyPreview::create()
                            .setName(mirror.name)
                            .setType(PropertyPreview::TypeEnum::Accessor)
                            .build();
    }
    if (!propertyPreview) continue;
    if (!*nameLimit) {
      *overflow = true;
      return;
    }
    --*nameLimit;
    privateProperties->emplace_back(std::move(propertyPreview));
  }
}

class ObjectMirror final : public ValueMirror {
 public:
  ObjectMirror(v8::Local<v8::Value> value, const String16& description)
      : m_value(value.As<v8::Object>()),
        m_description(description),
        m_hasSubtype(false) {}
  ObjectMirror(v8::Local<v8::Value> value, const String16& subtype,
               const String16& description)
      : m_value(value.As<v8::Object>()),
        m_description(description),
        m_hasSubtype(true),
        m_subtype(subtype) {}

  v8::Local<v8::Value> v8Value() const override { return m_value; }

  Response buildRemoteObject(
      v8::Local<v8::Context> context, WrapMode mode,
      std::unique_ptr<RemoteObject>* result) const override {
    if (mode == WrapMode::kForceValue) {
      std::unique_ptr<protocol::Value> protocolValue;
      Response response = toProtocolValue(context, m_value, &protocolValue);
      if (!response.IsSuccess()) return response;
      *result = RemoteObject::create()
                    .setType(RemoteObject::TypeEnum::Object)
                    .setValue(std::move(protocolValue))
                    .build();
    } else {
      v8::Isolate* isolate = context->GetIsolate();
      *result = RemoteObject::create()
                    .setType(RemoteObject::TypeEnum::Object)
                    .setClassName(toProtocolString(
                        isolate, m_value->GetConstructorName()))
                    .setDescription(m_description)
                    .build();
      if (m_hasSubtype) (*result)->setSubtype(m_subtype);
      if (mode == WrapMode::kWithPreview) {
        std::unique_ptr<ObjectPreview> previewValue;
        int nameLimit = 5;
        int indexLimit = 100;
        buildObjectPreview(context, false, &nameLimit, &indexLimit,
                           &previewValue);
        (*result)->setPreview(std::move(previewValue));
      }
    }
    return Response::Success();
  }

  void buildObjectPreview(
      v8::Local<v8::Context> context, bool generatePreviewForTable,
      int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* result) const override {
    buildObjectPreviewInternal(context, false /* forEntry */,
                               generatePreviewForTable, nameLimit, indexLimit,
                               result);
  }

  void buildEntryPreview(
      v8::Local<v8::Context> context, int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* result) const override {
    buildObjectPreviewInternal(context, true /* forEntry */,
                               false /* generatePreviewForTable */, nameLimit,
                               indexLimit, result);
  }

  void buildPropertyPreview(
      v8::Local<v8::Context> context, const String16& name,
      std::unique_ptr<PropertyPreview>* result) const override {
    *result = PropertyPreview::create()
                  .setName(name)
                  .setType(RemoteObject::TypeEnum::Object)
                  .setValue(abbreviateString(
                      m_description,
                      m_subtype == RemoteObject::SubtypeEnum::Regexp ? kMiddle
                                                                     : kEnd))
                  .build();
    if (m_hasSubtype) (*result)->setSubtype(m_subtype);
  }

 private:
  void buildObjectPreviewInternal(
      v8::Local<v8::Context> context, bool forEntry,
      bool generatePreviewForTable, int* nameLimit, int* indexLimit,
      std::unique_ptr<ObjectPreview>* result) const {
    auto properties = std::make_unique<protocol::Array<PropertyPreview>>();
    std::unique_ptr<protocol::Array<EntryPreview>> entriesPreview;
    bool overflow = false;

    v8::Local<v8::Value> value = m_value;
    while (value->IsProxy()) value = value.As<v8::Proxy>()->GetTarget();

    if (value->IsObject() && !value->IsProxy()) {
      v8::Local<v8::Object> objectForPreview = value.As<v8::Object>();
      std::vector<InternalPropertyMirror> internalProperties;
      getInternalPropertiesForPreview(context, objectForPreview, nameLimit,
                                      &overflow, &internalProperties);
      for (size_t i = 0; i < internalProperties.size(); ++i) {
        std::unique_ptr<PropertyPreview> propertyPreview;
        internalProperties[i].value->buildPropertyPreview(
            context, internalProperties[i].name, &propertyPreview);
        if (propertyPreview) {
          properties->emplace_back(std::move(propertyPreview));
        }
      }

      getPrivatePropertiesForPreview(context, objectForPreview, nameLimit,
                                     &overflow, properties.get());

      std::vector<PropertyMirror> mirrors;
      if (getPropertiesForPreview(context, objectForPreview, nameLimit,
                                  indexLimit, &overflow, &mirrors)) {
        for (size_t i = 0; i < mirrors.size(); ++i) {
          std::unique_ptr<PropertyPreview> preview;
          std::unique_ptr<ObjectPreview> valuePreview;
          if (mirrors[i].value) {
            mirrors[i].value->buildPropertyPreview(context, mirrors[i].name,
                                                   &preview);
            if (generatePreviewForTable) {
              int tableLimit = 1000;
              mirrors[i].value->buildObjectPreview(context, false, &tableLimit,
                                                   &tableLimit, &valuePreview);
            }
          } else {
            preview = PropertyPreview::create()
                          .setName(mirrors[i].name)
                          .setType(PropertyPreview::TypeEnum::Accessor)
                          .build();
          }
          if (valuePreview) {
            preview->setValuePreview(std::move(valuePreview));
          }
          properties->emplace_back(std::move(preview));
        }
      }

      std::vector<EntryMirror> entries;
      if (EntryMirror::getEntries(context, objectForPreview, 5, &overflow,
                                  &entries)) {
        if (forEntry) {
          overflow = true;
        } else {
          entriesPreview = std::make_unique<protocol::Array<EntryPreview>>();
          for (const auto& entry : entries) {
            std::unique_ptr<ObjectPreview> valuePreview;
            entry.value->buildEntryPreview(context, nameLimit, indexLimit,
                                           &valuePreview);
            if (!valuePreview) continue;
            std::unique_ptr<ObjectPreview> keyPreview;
            if (entry.key) {
              entry.key->buildEntryPreview(context, nameLimit, indexLimit,
                                           &keyPreview);
              if (!keyPreview) continue;
            }
            std::unique_ptr<EntryPreview> entryPreview =
                EntryPreview::create()
                    .setValue(std::move(valuePreview))
                    .build();
            if (keyPreview) entryPreview->setKey(std::move(keyPreview));
            entriesPreview->emplace_back(std::move(entryPreview));
          }
        }
      }
    }
    *result = ObjectPreview::create()
                  .setType(RemoteObject::TypeEnum::Object)
                  .setDescription(m_description)
                  .setOverflow(overflow)
                  .setProperties(std::move(properties))
                  .build();
    if (m_hasSubtype) (*result)->setSubtype(m_subtype);
    if (entriesPreview) (*result)->setEntries(std::move(entriesPreview));
  }

  v8::Local<v8::Object> m_value;
  String16 m_description;
  bool m_hasSubtype;
  String16 m_subtype;
};

void nativeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> data = info.Data().As<v8::Object>();
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> name;
  if (!data->GetRealNamedProperty(context, toV8String(isolate, "name"))
           .ToLocal(&name)) {
    return;
  }
  v8::Local<v8::Value> object;
  if (!data->GetRealNamedProperty(context, toV8String(isolate, "object"))
           .ToLocal(&object) ||
      !object->IsObject()) {
    return;
  }
  v8::Local<v8::Value> value;
  if (!object.As<v8::Object>()->Get(context, name).ToLocal(&value)) return;
  info.GetReturnValue().Set(value);
}

std::unique_ptr<ValueMirror> createNativeGetter(v8::Local<v8::Context> context,
                                                v8::Local<v8::Value> object,
                                                v8::Local<v8::Name> name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  if (data->Set(context, toV8String(isolate, "name"), name).IsNothing()) {
    return nullptr;
  }
  if (data->Set(context, toV8String(isolate, "object"), object).IsNothing()) {
    return nullptr;
  }

  v8::Local<v8::Function> function;
  if (!v8::Function::New(context, nativeGetterCallback, data, 0,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&function)) {
    return nullptr;
  }
  return ValueMirror::create(context, function);
}

void nativeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1) return;
  v8::Local<v8::Object> data = info.Data().As<v8::Object>();
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> name;
  if (!data->GetRealNamedProperty(context, toV8String(isolate, "name"))
           .ToLocal(&name)) {
    return;
  }
  v8::Local<v8::Value> object;
  if (!data->GetRealNamedProperty(context, toV8String(isolate, "object"))
           .ToLocal(&object) ||
      !object->IsObject()) {
    return;
  }
  v8::Local<v8::Value> value;
  if (!object.As<v8::Object>()->Set(context, name, info[0]).IsNothing()) return;
}

std::unique_ptr<ValueMirror> createNativeSetter(v8::Local<v8::Context> context,
                                                v8::Local<v8::Value> object,
                                                v8::Local<v8::Name> name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  if (data->Set(context, toV8String(isolate, "name"), name).IsNothing()) {
    return nullptr;
  }
  if (data->Set(context, toV8String(isolate, "object"), object).IsNothing()) {
    return nullptr;
  }

  v8::Local<v8::Function> function;
  if (!v8::Function::New(context, nativeSetterCallback, data, 1,
                         v8::ConstructorBehavior::kThrow)
           .ToLocal(&function)) {
    return nullptr;
  }
  return ValueMirror::create(context, function);
}

bool doesAttributeHaveObservableSideEffectOnGet(v8::Local<v8::Context> context,
                                                v8::Local<v8::Object> object,
                                                v8::Local<v8::Name> name) {
  // TODO(dgozman): we should remove this, annotate more embedder properties as
  // side-effect free, and call all getters which do not produce side effects.
  if (!name->IsString()) return false;
  v8::Isolate* isolate = context->GetIsolate();
  if (!name.As<v8::String>()->StringEquals(toV8String(isolate, "body"))) {
    return false;
  }

  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::Value> request;
  if (context->Global()
          ->GetRealNamedProperty(context, toV8String(isolate, "Request"))
          .ToLocal(&request)) {
    if (request->IsObject() &&
        object->InstanceOf(context, request.As<v8::Object>())
            .FromMaybe(false)) {
      return true;
    }
  }
  if (tryCatch.HasCaught()) tryCatch.Reset();

  v8::Local<v8::Value> response;
  if (context->Global()
          ->GetRealNamedProperty(context, toV8String(isolate, "Response"))
          .ToLocal(&response)) {
    if (response->IsObject() &&
        object->InstanceOf(context, response.As<v8::Object>())
            .FromMaybe(false)) {
      return true;
    }
  }
  return false;
}
template <typename ArrayView, typename ArrayBuffer>
void addTypedArrayView(v8::Local<v8::Context> context,
                       v8::Local<ArrayBuffer> buffer, size_t length,
                       const char* name,
                       ValueMirror::PropertyAccumulator* accumulator) {
  accumulator->Add(PropertyMirror{
      String16(name), false, false, false, true, false,
      ValueMirror::create(context, ArrayView::New(buffer, 0, length)), nullptr,
      nullptr, nullptr, nullptr});
}

template <typename ArrayBuffer>
void addTypedArrayViews(v8::Local<v8::Context> context,
                        v8::Local<ArrayBuffer> buffer,
                        ValueMirror::PropertyAccumulator* accumulator) {
  // TODO(alph): these should be internal properties.
  // TODO(v8:9308): Reconsider how large arrays are previewed.
  const size_t byte_length = buffer->ByteLength();

  size_t length = byte_length;
  if (length > v8::TypedArray::kMaxLength) return;

  addTypedArrayView<v8::Int8Array>(context, buffer, length, "[[Int8Array]]",
                                   accumulator);
  addTypedArrayView<v8::Uint8Array>(context, buffer, length, "[[Uint8Array]]",
                                    accumulator);

  length = byte_length / 2;
  if (length > v8::TypedArray::kMaxLength || (byte_length % 2) != 0) return;

  addTypedArrayView<v8::Int16Array>(context, buffer, length, "[[Int16Array]]",
                                    accumulator);

  length = byte_length / 4;
  if (length > v8::TypedArray::kMaxLength || (byte_length % 4) != 0) return;

  addTypedArrayView<v8::Int32Array>(context, buffer, length, "[[Int32Array]]",
                                    accumulator);
}
}  // anonymous namespace

ValueMirror::~ValueMirror() = default;

// static
bool ValueMirror::getProperties(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> object,
                                bool ownProperties, bool accessorPropertiesOnly,
                                PropertyAccumulator* accumulator) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::Set> set = v8::Set::New(isolate);

  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  V8InternalValueType internalType = v8InternalValueTypeFrom(context, object);
  if (internalType == V8InternalValueType::kScope) {
    v8::Local<v8::Value> value;
    if (!object->Get(context, toV8String(isolate, "object")).ToLocal(&value) ||
        !value->IsObject()) {
      return false;
    } else {
      object = value.As<v8::Object>();
    }
  }
  if (internalType == V8InternalValueType::kScopeList) {
    if (!set->Add(context, toV8String(isolate, "length")).ToLocal(&set)) {
      return false;
    }
  }
  bool shouldSkipProto = internalType == V8InternalValueType::kScopeList;

  bool formatAccessorsAsProperties =
      clientFor(context)->formatAccessorsAsProperties(object);

  if (object->IsArrayBuffer()) {
    addTypedArrayViews(context, object.As<v8::ArrayBuffer>(), accumulator);
  }
  if (object->IsSharedArrayBuffer()) {
    addTypedArrayViews(context, object.As<v8::SharedArrayBuffer>(),
                       accumulator);
  }

  for (auto iterator = v8::debug::PropertyIterator::Create(object);
       !iterator->Done(); iterator->Advance()) {
    bool isOwn = iterator->is_own();
    if (!isOwn && ownProperties) break;
    v8::Local<v8::Name> v8Name = iterator->name();
    v8::Maybe<bool> result = set->Has(context, v8Name);
    if (result.IsNothing()) return false;
    if (result.FromJust()) continue;
    if (!set->Add(context, v8Name).ToLocal(&set)) return false;

    String16 name;
    std::unique_ptr<ValueMirror> symbolMirror;
    if (v8Name->IsString()) {
      name = toProtocolString(isolate, v8Name.As<v8::String>());
    } else {
      v8::Local<v8::Symbol> symbol = v8Name.As<v8::Symbol>();
      name = descriptionForSymbol(context, symbol);
      symbolMirror = ValueMirror::create(context, symbol);
    }

    v8::PropertyAttribute attributes;
    std::unique_ptr<ValueMirror> valueMirror;
    std::unique_ptr<ValueMirror> getterMirror;
    std::unique_ptr<ValueMirror> setterMirror;
    std::unique_ptr<ValueMirror> exceptionMirror;
    bool writable = false;
    bool enumerable = false;
    bool configurable = false;

    bool isAccessorProperty = false;
    v8::TryCatch tryCatch(isolate);
    if (!iterator->attributes().To(&attributes)) {
      exceptionMirror = ValueMirror::create(context, tryCatch.Exception());
    } else {
      if (iterator->is_native_accessor()) {
        if (iterator->has_native_getter()) {
          getterMirror = createNativeGetter(context, object, v8Name);
        }
        if (iterator->has_native_setter()) {
          setterMirror = createNativeSetter(context, object, v8Name);
        }
        writable = !(attributes & v8::PropertyAttribute::ReadOnly);
        enumerable = !(attributes & v8::PropertyAttribute::DontEnum);
        configurable = !(attributes & v8::PropertyAttribute::DontDelete);
        isAccessorProperty = getterMirror || setterMirror;
      } else {
        v8::TryCatch tryCatch(isolate);
        v8::debug::PropertyDescriptor descriptor;
        if (!iterator->descriptor().To(&descriptor)) {
          exceptionMirror = ValueMirror::create(context, tryCatch.Exception());
        } else {
          writable = descriptor.has_writable ? descriptor.writable : false;
          enumerable =
              descriptor.has_enumerable ? descriptor.enumerable : false;
          configurable =
              descriptor.has_configurable ? descriptor.configurable : false;
          if (!descriptor.value.IsEmpty()) {
            valueMirror = ValueMirror::create(context, descriptor.value);
          }
          bool getterIsNativeFunction = false;
          if (!descriptor.get.IsEmpty()) {
            v8::Local<v8::Value> get = descriptor.get;
            getterMirror = ValueMirror::create(context, get);
            getterIsNativeFunction =
                get->IsFunction() && get.As<v8::Function>()->ScriptId() ==
                                         v8::UnboundScript::kNoScriptId;
          }
          if (!descriptor.set.IsEmpty()) {
            setterMirror = ValueMirror::create(context, descriptor.set);
          }
          isAccessorProperty = getterMirror || setterMirror;
          bool isSymbolDescription =
              object->IsSymbol() && name == "description";
          if (isSymbolDescription ||
              (name != "__proto__" && getterIsNativeFunction &&
               formatAccessorsAsProperties &&
               !doesAttributeHaveObservableSideEffectOnGet(context, object,
                                                           v8Name))) {
            v8::TryCatch tryCatch(isolate);
            v8::Local<v8::Value> value;
            if (object->Get(context, v8Name).ToLocal(&value)) {
              valueMirror = ValueMirror::create(context, value);
              isOwn = true;
              setterMirror = nullptr;
              getterMirror = nullptr;
            }
          }
        }
      }
    }
    if (accessorPropertiesOnly && !isAccessorProperty) continue;
    auto mirror = PropertyMirror{name,
                                 writable,
                                 configurable,
                                 enumerable,
                                 isOwn,
                                 iterator->is_array_index(),
                                 std::move(valueMirror),
                                 std::move(getterMirror),
                                 std::move(setterMirror),
                                 std::move(symbolMirror),
                                 std::move(exceptionMirror)};
    if (!accumulator->Add(std::move(mirror))) return true;
  }
  if (!shouldSkipProto && ownProperties && !object->IsProxy() &&
      !accessorPropertiesOnly) {
    v8::Local<v8::Value> prototype = object->GetPrototype();
    if (prototype->IsObject()) {
      accumulator->Add(PropertyMirror{String16("__proto__"), true, true, false,
                                      true, false,
                                      ValueMirror::create(context, prototype),
                                      nullptr, nullptr, nullptr, nullptr});
    }
  }
  return true;
}

// static
void ValueMirror::getInternalProperties(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object,
    std::vector<InternalPropertyMirror>* mirrors) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch tryCatch(isolate);
  if (object->IsFunction()) {
    v8::Local<v8::Function> function = object.As<v8::Function>();
    auto location = LocationMirror::create(function);
    if (location) {
      mirrors->emplace_back(InternalPropertyMirror{
          String16("[[FunctionLocation]]"), std::move(location)});
    }
    if (function->IsGeneratorFunction()) {
      mirrors->emplace_back(InternalPropertyMirror{
          String16("[[IsGenerator]]"),
          ValueMirror::create(context, v8::True(context->GetIsolate()))});
    }
  }
  if (object->IsGeneratorObject()) {
    auto location = LocationMirror::createForGenerator(object);
    if (location) {
      mirrors->emplace_back(InternalPropertyMirror{
          String16("[[GeneratorLocation]]"), std::move(location)});
    }
  }
  V8Debugger* debugger =
      static_cast<V8InspectorImpl*>(v8::debug::GetInspector(isolate))
          ->debugger();
  v8::Local<v8::Array> properties;
  if (debugger->internalProperties(context, object).ToLocal(&properties)) {
    for (uint32_t i = 0; i < properties->Length(); i += 2) {
      v8::Local<v8::Value> name;
      if (!properties->Get(context, i).ToLocal(&name) || !name->IsString()) {
        tryCatch.Reset();
        continue;
      }
      v8::Local<v8::Value> value;
      if (!properties->Get(context, i + 1).ToLocal(&value)) {
        tryCatch.Reset();
        continue;
      }
      auto wrapper = ValueMirror::create(context, value);
      if (wrapper) {
        mirrors->emplace_back(InternalPropertyMirror{
            toProtocolStringWithTypeCheck(context->GetIsolate(), name),
            std::move(wrapper)});
      }
    }
  }
}

// static
std::vector<PrivatePropertyMirror> ValueMirror::getPrivateProperties(
    v8::Local<v8::Context> context, v8::Local<v8::Object> object) {
  std::vector<PrivatePropertyMirror> mirrors;
  v8::Isolate* isolate = context->GetIsolate();
  v8::MicrotasksScope microtasksScope(isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::Array> privateProperties;

  std::vector<v8::Local<v8::Value>> names;
  std::vector<v8::Local<v8::Value>> values;
  if (!v8::debug::GetPrivateMembers(context, object, &names, &values))
    return mirrors;

  size_t len = values.size();
  for (size_t i = 0; i < len; i++) {
    v8::Local<v8::Value> name = names[i];
    DCHECK(name->IsString());
    v8::Local<v8::Value> value = values[i];

    std::unique_ptr<ValueMirror> valueMirror;
    std::unique_ptr<ValueMirror> getterMirror;
    std::unique_ptr<ValueMirror> setterMirror;
    if (v8::debug::AccessorPair::IsAccessorPair(value)) {
      v8::Local<v8::debug::AccessorPair> accessors =
          value.As<v8::debug::AccessorPair>();
      v8::Local<v8::Value> getter = accessors->getter();
      v8::Local<v8::Value> setter = accessors->setter();
      if (!getter->IsNull()) {
        getterMirror = ValueMirror::create(context, getter);
      }
      if (!setter->IsNull()) {
        setterMirror = ValueMirror::create(context, setter);
      }
    } else {
      valueMirror = ValueMirror::create(context, value);
    }

    mirrors.emplace_back(PrivatePropertyMirror{
        toProtocolStringWithTypeCheck(context->GetIsolate(), name),
        std::move(valueMirror), std::move(getterMirror),
        std::move(setterMirror)});
  }
  return mirrors;
}

String16 descriptionForNode(v8::Local<v8::Context> context,
                            v8::Local<v8::Value> value) {
  if (!value->IsObject()) return String16();
  v8::Local<v8::Object> object = value.As<v8::Object>();
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::Value> nodeName;
  if (!object->Get(context, toV8String(isolate, "nodeName"))
           .ToLocal(&nodeName)) {
    return String16();
  }
  String16 description;
  v8::Local<v8::Function> toLowerCase =
      v8::debug::GetBuiltin(isolate, v8::debug::kStringToLowerCase);
  if (nodeName->IsString()) {
    if (!toLowerCase->Call(context, nodeName, 0, nullptr).ToLocal(&nodeName))
      return String16();
    if (nodeName->IsString()) {
      description = toProtocolString(isolate, nodeName.As<v8::String>());
    }
  }
  if (!description.length()) {
    v8::Local<v8::Value> value;
    if (!object->Get(context, toV8String(isolate, "constructor"))
             .ToLocal(&value) ||
        !value->IsObject()) {
      return String16();
    }
    if (!value.As<v8::Object>()
             ->Get(context, toV8String(isolate, "name"))
             .ToLocal(&value) ||
        !value->IsString()) {
      return String16();
    }
    description = toProtocolString(isolate, value.As<v8::String>());
  }
  v8::Local<v8::Value> nodeType;
  if (!object->Get(context, toV8String(isolate, "nodeType"))
           .ToLocal(&nodeType) ||
      !nodeType->IsInt32()) {
    return description;
  }
  if (nodeType.As<v8::Int32>()->Value() == 1) {
    v8::Local<v8::Value> idValue;
    if (!object->Get(context, toV8String(isolate, "id")).ToLocal(&idValue)) {
      return description;
    }
    if (idValue->IsString()) {
      String16 id = toProtocolString(isolate, idValue.As<v8::String>());
      if (id.length()) {
        description = String16::concat(description, '#', id);
      }
    }
    v8::Local<v8::Value> classNameValue;
    if (!object->Get(context, toV8String(isolate, "className"))
             .ToLocal(&classNameValue)) {
      return description;
    }
    if (classNameValue->IsString() &&
        classNameValue.As<v8::String>()->Length()) {
      String16 classes =
          toProtocolString(isolate, classNameValue.As<v8::String>());
      String16Builder output;
      bool previousIsDot = false;
      for (size_t i = 0; i < classes.length(); ++i) {
        if (classes[i] == ' ') {
          if (!previousIsDot) {
            output.append('.');
            previousIsDot = true;
          }
        } else {
          output.append(classes[i]);
          previousIsDot = classes[i] == '.';
        }
      }
      description = String16::concat(description, '.', output.toString());
    }
  } else if (nodeType.As<v8::Int32>()->Value() == 1) {
    return String16::concat("<!DOCTYPE ", description, '>');
  }
  return description;
}

std::unique_ptr<ValueMirror> clientMirror(v8::Local<v8::Context> context,
                                          v8::Local<v8::Value> value,
                                          const String16& subtype) {
  // TODO(alph): description and length retrieval should move to embedder.
  if (subtype == "node") {
    return std::make_unique<ObjectMirror>(value, subtype,
                                          descriptionForNode(context, value));
  }
  if (subtype == "error") {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Error,
        descriptionForError(context, value.As<v8::Object>(),
                            ErrorType::kClient));
  }
  if (subtype == "array" && value->IsObject()) {
    v8::Isolate* isolate = context->GetIsolate();
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Object> object = value.As<v8::Object>();
    v8::Local<v8::Value> lengthValue;
    if (object->Get(context, toV8String(isolate, "length"))
            .ToLocal(&lengthValue)) {
      if (lengthValue->IsInt32()) {
        return std::make_unique<ObjectMirror>(
            value, RemoteObject::SubtypeEnum::Array,
            descriptionForCollection(isolate, object,
                                     lengthValue.As<v8::Int32>()->Value()));
      }
    }
  }
  return std::make_unique<ObjectMirror>(
      value,
      descriptionForObject(context->GetIsolate(), value.As<v8::Object>()));
}

std::unique_ptr<ValueMirror> ValueMirror::create(v8::Local<v8::Context> context,
                                                 v8::Local<v8::Value> value) {
  if (value->IsNull()) {
    return std::make_unique<PrimitiveValueMirror>(
        value, RemoteObject::TypeEnum::Object);
  }
  if (value->IsBoolean()) {
    return std::make_unique<PrimitiveValueMirror>(
        value, RemoteObject::TypeEnum::Boolean);
  }
  if (value->IsNumber()) {
    return std::make_unique<NumberMirror>(value.As<v8::Number>());
  }
  v8::Isolate* isolate = context->GetIsolate();
  if (value->IsString()) {
    return std::make_unique<PrimitiveValueMirror>(
        value, RemoteObject::TypeEnum::String);
  }
  if (value->IsBigInt()) {
    return std::make_unique<BigIntMirror>(value.As<v8::BigInt>());
  }
  if (value->IsSymbol()) {
    return std::make_unique<SymbolMirror>(value.As<v8::Symbol>());
  }
  if (v8::debug::WasmValue::IsWasmValue(value)) {
    return std::make_unique<WasmValueMirror>(value.As<v8::debug::WasmValue>());
  }
  auto clientSubtype = (value->IsUndefined() || value->IsObject())
                           ? clientFor(context)->valueSubtype(value)
                           : nullptr;
  if (clientSubtype) {
    String16 subtype = toString16(clientSubtype->string());
    return clientMirror(context, value, subtype);
  }
  if (value->IsUndefined()) {
    return std::make_unique<PrimitiveValueMirror>(
        value, RemoteObject::TypeEnum::Undefined);
  }
  if (value->IsRegExp()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Regexp,
        descriptionForRegExp(isolate, value.As<v8::RegExp>()));
  }
  if (value->IsProxy()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Proxy, "Proxy");
  }
  if (value->IsFunction()) {
    return std::make_unique<FunctionMirror>(value);
  }
  if (value->IsDate()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Date,
        descriptionForDate(context, value.As<v8::Date>()));
  }
  if (value->IsPromise()) {
    v8::Local<v8::Promise> promise = value.As<v8::Promise>();
    return std::make_unique<ObjectMirror>(
        promise, RemoteObject::SubtypeEnum::Promise,
        descriptionForObject(isolate, promise));
  }
  if (value->IsNativeError()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Error,
        descriptionForError(context, value.As<v8::Object>(),
                            ErrorType::kNative));
  }
  if (value->IsMap()) {
    v8::Local<v8::Map> map = value.As<v8::Map>();
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Map,
        descriptionForCollection(isolate, map, map->Size()));
  }
  if (value->IsSet()) {
    v8::Local<v8::Set> set = value.As<v8::Set>();
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Set,
        descriptionForCollection(isolate, set, set->Size()));
  }
  if (value->IsWeakMap()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Weakmap,
        descriptionForObject(isolate, value.As<v8::Object>()));
  }
  if (value->IsWeakSet()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Weakset,
        descriptionForObject(isolate, value.As<v8::Object>()));
  }
  if (value->IsMapIterator() || value->IsSetIterator()) {
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Iterator,
        descriptionForObject(isolate, value.As<v8::Object>()));
  }
  if (value->IsGeneratorObject()) {
    v8::Local<v8::Object> object = value.As<v8::Object>();
    return std::make_unique<ObjectMirror>(
        object, RemoteObject::SubtypeEnum::Generator,
        descriptionForObject(isolate, object));
  }
  if (value->IsTypedArray()) {
    v8::Local<v8::TypedArray> array = value.As<v8::TypedArray>();
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Typedarray,
        descriptionForCollection(isolate, array, array->Length()));
  }
  if (value->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Arraybuffer,
        descriptionForCollection(isolate, buffer, buffer->ByteLength()));
  }
  if (value->IsSharedArrayBuffer()) {
    v8::Local<v8::SharedArrayBuffer> buffer = value.As<v8::SharedArrayBuffer>();
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Arraybuffer,
        descriptionForCollection(isolate, buffer, buffer->ByteLength()));
  }
  if (value->IsDataView()) {
    v8::Local<v8::DataView> view = value.As<v8::DataView>();
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Dataview,
        descriptionForCollection(isolate, view, view->ByteLength()));
  }
  V8InternalValueType internalType =
      v8InternalValueTypeFrom(context, v8::Local<v8::Object>::Cast(value));
  if (value->IsArray() && internalType == V8InternalValueType::kScopeList) {
    return std::make_unique<ObjectMirror>(
        value, "internal#scopeList",
        descriptionForScopeList(value.As<v8::Array>()));
  }
  if (value->IsObject() && internalType == V8InternalValueType::kEntry) {
    return std::make_unique<ObjectMirror>(
        value, "internal#entry",
        descriptionForEntry(context, value.As<v8::Object>()));
  }
  if (value->IsObject() && internalType == V8InternalValueType::kScope) {
    return std::make_unique<ObjectMirror>(
        value, "internal#scope",
        descriptionForScope(context, value.As<v8::Object>()));
  }
  size_t length = 0;
  if (value->IsArray() || isArrayLike(context, value, &length)) {
    length = value->IsArray() ? value.As<v8::Array>()->Length() : length;
    return std::make_unique<ObjectMirror>(
        value, RemoteObject::SubtypeEnum::Array,
        descriptionForCollection(isolate, value.As<v8::Object>(), length));
  }
  if (value->IsObject()) {
    return std::make_unique<ObjectMirror>(
        value, descriptionForObject(isolate, value.As<v8::Object>()));
  }
  return nullptr;
}
}  // namespace v8_inspector
