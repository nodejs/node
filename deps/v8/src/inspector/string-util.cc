// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/string-util.h"

#include <cinttypes>
#include <cmath>

#include "src/base/platform/platform.h"
#include "src/inspector/protocol/Protocol.h"
#include "src/numbers/conversions.h"

namespace v8_inspector {

v8::Local<v8::String> toV8String(v8::Isolate* isolate, const String16& string) {
  if (string.isEmpty()) return v8::String::Empty(isolate);
  DCHECK_GT(v8::String::kMaxLength, string.length());
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.characters16()),
             v8::NewStringType::kNormal, static_cast<int>(string.length()))
      .ToLocalChecked();
}

v8::Local<v8::String> toV8StringInternalized(v8::Isolate* isolate,
                                             const String16& string) {
  if (string.isEmpty()) return v8::String::Empty(isolate);
  DCHECK_GT(v8::String::kMaxLength, string.length());
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
  DCHECK_GT(v8::String::kMaxLength, string.length());
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

String16 toProtocolString(v8::Isolate* isolate, v8::Local<v8::String> value) {
  if (value.IsEmpty() || value->IsNullOrUndefined()) return String16();
  std::unique_ptr<UChar[]> buffer(new UChar[value->Length()]);
  value->Write(isolate, reinterpret_cast<uint16_t*>(buffer.get()), 0,
               value->Length());
  return String16(buffer.get(), value->Length());
}

String16 toProtocolStringWithTypeCheck(v8::Isolate* isolate,
                                       v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsString()) return String16();
  return toProtocolString(isolate, value.As<v8::String>());
}

String16 toString16(const StringView& string) {
  if (!string.length()) return String16();
  if (string.is8Bit())
    return String16(reinterpret_cast<const char*>(string.characters8()),
                    string.length());
  return String16(string.characters16(), string.length());
}

StringView toStringView(const String16& string) {
  if (string.isEmpty()) return StringView();
  return StringView(string.characters16(), string.length());
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

// static
double StringUtil::toDouble(const char* s, size_t len, bool* isOk) {
  int flags = v8::internal::ALLOW_HEX | v8::internal::ALLOW_OCTAL |
              v8::internal::ALLOW_BINARY;
  double result = v8::internal::StringToDouble(s, flags);
  *isOk = !std::isnan(result);
  return result;
}

std::unique_ptr<protocol::Value> StringUtil::parseJSON(
    const StringView& string) {
  if (!string.length()) return nullptr;
  if (string.is8Bit()) {
    return parseJSONCharacters(string.characters8(),
                               static_cast<int>(string.length()));
  }
  return parseJSONCharacters(string.characters16(),
                             static_cast<int>(string.length()));
}

std::unique_ptr<protocol::Value> StringUtil::parseJSON(const String16& string) {
  if (!string.length()) return nullptr;
  return parseJSONCharacters(string.characters16(),
                             static_cast<int>(string.length()));
}
}  // namespace protocol

namespace {
// An empty string buffer doesn't own any string data; its ::string() returns a
// default-constructed StringView instance.
class EmptyStringBuffer : public StringBuffer {
 public:
  StringView string() const override { return StringView(); }
};

// Contains LATIN1 text data or CBOR encoded binary data in a vector.
class StringBuffer8 : public StringBuffer {
 public:
  explicit StringBuffer8(std::vector<uint8_t> data) : data_(std::move(data)) {}

  StringView string() const override {
    return StringView(data_.data(), data_.size());
  }

 private:
  std::vector<uint8_t> data_;
};

// Contains a 16 bit string (String16).
class StringBuffer16 : public StringBuffer {
 public:
  explicit StringBuffer16(String16 data) : data_(std::move(data)) {}

  StringView string() const override {
    return StringView(data_.characters16(), data_.length());
  }

 private:
  String16 data_;
};
}  // namespace

// static
std::unique_ptr<StringBuffer> StringBuffer::create(StringView string) {
  if (string.length() == 0) return std::make_unique<EmptyStringBuffer>();
  if (string.is8Bit()) {
    return std::make_unique<StringBuffer8>(std::vector<uint8_t>(
        string.characters8(), string.characters8() + string.length()));
  }
  return std::make_unique<StringBuffer16>(
      String16(string.characters16(), string.length()));
}

std::unique_ptr<StringBuffer> StringBufferFrom(String16 str) {
  if (str.isEmpty()) return std::make_unique<EmptyStringBuffer>();
  return std::make_unique<StringBuffer16>(std::move(str));
}

std::unique_ptr<StringBuffer> StringBufferFrom(std::vector<uint8_t> str) {
  if (str.empty()) return std::make_unique<EmptyStringBuffer>();
  return std::make_unique<StringBuffer8>(std::move(str));
}

String16 stackTraceIdToString(uintptr_t id) {
  String16Builder builder;
  builder.appendNumber(static_cast<size_t>(id));
  return builder.toString();
}

}  // namespace v8_inspector
