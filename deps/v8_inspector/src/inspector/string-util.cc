// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/string-util.h"

#include "src/inspector/protocol/Protocol.h"

namespace v8_inspector {

v8::Local<v8::String> toV8String(v8::Isolate* isolate, const String16& string) {
  if (string.isEmpty()) return v8::String::Empty(isolate);
  DCHECK(string.length() < v8::String::kMaxLength);
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.characters16()),
             v8::NewStringType::kNormal, static_cast<int>(string.length()))
      .ToLocalChecked();
}

v8::Local<v8::String> toV8StringInternalized(v8::Isolate* isolate,
                                             const String16& string) {
  if (string.isEmpty()) return v8::String::Empty(isolate);
  DCHECK(string.length() < v8::String::kMaxLength);
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.characters16()),
             v8::NewStringType::kInternalized,
             static_cast<int>(string.length()))
      .ToLocalChecked();
}

v8::Local<v8::String> toV8StringInternalized(v8::Isolate* isolate,
                                             const char* str) {
  return v8::String::NewFromUtf8(isolate, str, v8::NewStringType::kInternalized)
      .ToLocalChecked();
}

v8::Local<v8::String> toV8String(v8::Isolate* isolate,
                                 const StringView& string) {
  if (!string.length()) return v8::String::Empty(isolate);
  DCHECK(string.length() < v8::String::kMaxLength);
  if (string.is8Bit())
    return v8::String::NewFromOneByte(
               isolate, reinterpret_cast<const uint8_t*>(string.characters8()),
               v8::NewStringType::kNormal, static_cast<int>(string.length()))
        .ToLocalChecked();
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.characters16()),
             v8::NewStringType::kNormal, static_cast<int>(string.length()))
      .ToLocalChecked();
}

String16 toProtocolString(v8::Local<v8::String> value) {
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
    return String16();
  std::unique_ptr<UChar[]> buffer(new UChar[value->Length()]);
  value->Write(reinterpret_cast<uint16_t*>(buffer.get()), 0, value->Length());
  return String16(buffer.get(), value->Length());
}

String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsString()) return String16();
  return toProtocolString(value.As<v8::String>());
}

String16 toString16(const StringView& string) {
  if (!string.length()) return String16();
  if (string.is8Bit())
    return String16(reinterpret_cast<const char*>(string.characters8()),
                    string.length());
  return String16(reinterpret_cast<const UChar*>(string.characters16()),
                  string.length());
}

StringView toStringView(const String16& string) {
  if (string.isEmpty()) return StringView();
  return StringView(reinterpret_cast<const uint16_t*>(string.characters16()),
                    string.length());
}

bool stringViewStartsWith(const StringView& string, const char* prefix) {
  if (!string.length()) return !(*prefix);
  if (string.is8Bit()) {
    for (size_t i = 0, j = 0; prefix[j] && i < string.length(); ++i, ++j) {
      if (string.characters8()[i] != prefix[j]) return false;
    }
  } else {
    for (size_t i = 0, j = 0; prefix[j] && i < string.length(); ++i, ++j) {
      if (string.characters16()[i] != prefix[j]) return false;
    }
  }
  return true;
}

namespace protocol {

std::unique_ptr<protocol::Value> parseJSON(const StringView& string) {
  if (!string.length()) return nullptr;
  if (string.is8Bit()) {
    return protocol::parseJSON(string.characters8(),
                               static_cast<int>(string.length()));
  }
  return protocol::parseJSON(string.characters16(),
                             static_cast<int>(string.length()));
}

std::unique_ptr<protocol::Value> parseJSON(const String16& string) {
  if (!string.length()) return nullptr;
  return protocol::parseJSON(string.characters16(),
                             static_cast<int>(string.length()));
}

}  // namespace protocol

std::unique_ptr<protocol::Value> toProtocolValue(protocol::String* errorString,
                                                 v8::Local<v8::Context> context,
                                                 v8::Local<v8::Value> value,
                                                 int maxDepth) {
  if (value.IsEmpty()) {
    UNREACHABLE();
    return nullptr;
  }

  if (!maxDepth) {
    *errorString = "Object reference chain is too long";
    return nullptr;
  }
  maxDepth--;

  if (value->IsNull() || value->IsUndefined()) return protocol::Value::null();
  if (value->IsBoolean())
    return protocol::FundamentalValue::create(value.As<v8::Boolean>()->Value());
  if (value->IsNumber()) {
    double doubleValue = value.As<v8::Number>()->Value();
    int intValue = static_cast<int>(doubleValue);
    if (intValue == doubleValue)
      return protocol::FundamentalValue::create(intValue);
    return protocol::FundamentalValue::create(doubleValue);
  }
  if (value->IsString())
    return protocol::StringValue::create(
        toProtocolString(value.As<v8::String>()));
  if (value->IsArray()) {
    v8::Local<v8::Array> array = value.As<v8::Array>();
    std::unique_ptr<protocol::ListValue> inspectorArray =
        protocol::ListValue::create();
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; i++) {
      v8::Local<v8::Value> value;
      if (!array->Get(context, i).ToLocal(&value)) {
        *errorString = "Internal error";
        return nullptr;
      }
      std::unique_ptr<protocol::Value> element =
          toProtocolValue(errorString, context, value, maxDepth);
      if (!element) return nullptr;
      inspectorArray->pushValue(std::move(element));
    }
    return std::move(inspectorArray);
  }
  if (value->IsObject()) {
    std::unique_ptr<protocol::DictionaryValue> jsonObject =
        protocol::DictionaryValue::create();
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
    v8::Local<v8::Array> propertyNames;
    if (!object->GetPropertyNames(context).ToLocal(&propertyNames)) {
      *errorString = "Internal error";
      return nullptr;
    }
    uint32_t length = propertyNames->Length();
    for (uint32_t i = 0; i < length; i++) {
      v8::Local<v8::Value> name;
      if (!propertyNames->Get(context, i).ToLocal(&name)) {
        *errorString = "Internal error";
        return nullptr;
      }
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
      if (!object->Get(context, name).ToLocal(&property)) {
        *errorString = "Internal error";
        return nullptr;
      }
      std::unique_ptr<protocol::Value> propertyValue =
          toProtocolValue(errorString, context, property, maxDepth);
      if (!propertyValue) return nullptr;
      jsonObject->setValue(toProtocolString(propertyName),
                           std::move(propertyValue));
    }
    return std::move(jsonObject);
  }
  *errorString = "Object couldn't be returned by value";
  return nullptr;
}

// static
std::unique_ptr<StringBuffer> StringBuffer::create(const StringView& string) {
  String16 owner = toString16(string);
  return StringBufferImpl::adopt(owner);
}

// static
std::unique_ptr<StringBufferImpl> StringBufferImpl::adopt(String16& string) {
  return wrapUnique(new StringBufferImpl(string));
}

StringBufferImpl::StringBufferImpl(String16& string) {
  m_owner.swap(string);
  m_string = toStringView(m_owner);
}

}  // namespace v8_inspector
