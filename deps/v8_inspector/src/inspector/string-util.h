// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_STRINGUTIL_H_
#define V8_INSPECTOR_STRINGUTIL_H_

#include "src/base/macros.h"
#include "src/inspector/string-16.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace protocol {

class Value;

using String = v8_inspector::String16;
using StringBuilder = v8_inspector::String16Builder;

class StringUtil {
 public:
  static String substring(const String& s, size_t pos, size_t len) {
    return s.substring(pos, len);
  }
  static String fromInteger(int number) { return String::fromInteger(number); }
  static String fromInteger(size_t number) {
    return String::fromInteger(number);
  }
  static String fromDouble(double number) { return String::fromDouble(number); }
  static const size_t kNotFound = String::kNotFound;
  static void builderReserve(StringBuilder& builder, size_t capacity) {
    builder.reserveCapacity(capacity);
  }
};

std::unique_ptr<protocol::Value> parseJSON(const StringView& json);
std::unique_ptr<protocol::Value> parseJSON(const String16& json);

}  // namespace protocol

std::unique_ptr<protocol::Value> toProtocolValue(protocol::String* errorString,
                                                 v8::Local<v8::Context>,
                                                 v8::Local<v8::Value>,
                                                 int maxDepth = 1000);

v8::Local<v8::String> toV8String(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const char*);
v8::Local<v8::String> toV8String(v8::Isolate*, const StringView&);
// TODO(dgozman): rename to toString16.
String16 toProtocolString(v8::Local<v8::String>);
String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value>);
String16 toString16(const StringView&);
StringView toStringView(const String16&);
bool stringViewStartsWith(const StringView&, const char*);

class StringBufferImpl : public StringBuffer {
 public:
  // Destroys string's content.
  static std::unique_ptr<StringBufferImpl> adopt(String16&);
  const StringView& string() override { return m_string; }

 private:
  explicit StringBufferImpl(String16&);
  String16 m_owner;
  StringView m_string;

  DISALLOW_COPY_AND_ASSIGN(StringBufferImpl);
};

}  //  namespace v8_inspector

#endif  // V8_INSPECTOR_STRINGUTIL_H_
