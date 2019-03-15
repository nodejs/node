// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/string-util.h"

#include <cmath>

#include "src/base/platform/platform.h"
#include "src/conversions.h"
#include "src/inspector/protocol/Protocol.h"

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

// static
std::unique_ptr<protocol::Value> StringUtil::parseProtocolMessage(
    const ProtocolMessage& message) {
  return parseJSON(message.json);
}

// static
ProtocolMessage StringUtil::jsonToMessage(String message) {
  ProtocolMessage result;
  result.json = std::move(message);
  return result;
}

// static
ProtocolMessage StringUtil::binaryToMessage(std::vector<uint8_t> message) {
  ProtocolMessage result;
  result.binary = std::move(message);
  return result;
}

// static
void StringUtil::builderAppendQuotedString(StringBuilder& builder,
                                           const String& str) {
  builder.append('"');
  if (!str.isEmpty()) {
    escapeWideStringForJSON(
        reinterpret_cast<const uint16_t*>(str.characters16()),
        static_cast<int>(str.length()), &builder);
  }
  builder.append('"');
}

}  // namespace protocol

// static
std::unique_ptr<StringBuffer> StringBuffer::create(const StringView& string) {
  String16 owner = toString16(string);
  return StringBufferImpl::adopt(owner);
}

// static
std::unique_ptr<StringBufferImpl> StringBufferImpl::adopt(String16& string) {
  return std::unique_ptr<StringBufferImpl>(new StringBufferImpl(string));
}

StringBufferImpl::StringBufferImpl(String16& string) {
  m_owner.swap(string);
  m_string = toStringView(m_owner);
}

String16 debuggerIdToString(const std::pair<int64_t, int64_t>& debuggerId) {
  const size_t kBufferSize = 35;

  char buffer[kBufferSize];
  v8::base::OS::SNPrintF(buffer, kBufferSize, "(%08" PRIX64 "%08" PRIX64 ")",
                         debuggerId.first, debuggerId.second);
  return String16(buffer);
}

String16 stackTraceIdToString(uintptr_t id) {
  String16Builder builder;
  builder.appendNumber(static_cast<size_t>(id));
  return builder.toString();
}

}  // namespace v8_inspector
