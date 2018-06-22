// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tracing/traced_value.h"

#include <math.h>
#include <sstream>
#include <stdio.h>
#include <string>

#if defined(_STLP_VENDOR_CSTD)
// STLPort doesn't import fpclassify into the std namespace.
#define FPCLASSIFY_NAMESPACE
#else
#define FPCLASSIFY_NAMESPACE std
#endif

namespace node {
namespace tracing {

namespace {

void EscapeAndAppendString(const char* value, std::string* result) {
  *result += '"';
  char number_buffer[10];
  while (*value) {
    char c = *value++;
    switch (c) {
      case '\t':
        *result += "\\t";
        break;
      case '\n':
        *result += "\\n";
        break;
      case '\"':
        *result += "\\\"";
        break;
      case '\\':
        *result += "\\\\";
        break;
      default:
        if (c < '\x20') {
          snprintf(
              number_buffer, arraysize(number_buffer), "\\u%04X",
              static_cast<unsigned>(static_cast<unsigned char>(c)));
          *result += number_buffer;
        } else {
          *result += c;
        }
    }
  }
  *result += '"';
}

std::string DoubleToCString(double v) {
  switch (FPCLASSIFY_NAMESPACE::fpclassify(v)) {
    case FP_NAN: return "\"NaN\"";
    case FP_INFINITE: return (v < 0.0 ? "\"-Infinity\"" : "\"Infinity\"");
    case FP_ZERO: return "0";
    default:
      // This is a far less sophisticated version than the one used inside v8.
      std::ostringstream stream;
      stream.imbue(std::locale("C"));  // Ignore locale
      stream << v;
      return stream.str();
  }
}

}  // namespace

std::unique_ptr<TracedValue> TracedValue::Create() {
  return std::unique_ptr<TracedValue>(new TracedValue(false));
}

std::unique_ptr<TracedValue> TracedValue::CreateArray() {
  return std::unique_ptr<TracedValue>(new TracedValue(true));
}

TracedValue::TracedValue(bool root_is_array) :
    first_item_(true), root_is_array_(root_is_array) {}

TracedValue::~TracedValue() {}

void TracedValue::SetInteger(const char* name, int value) {
  WriteName(name);
  data_ += std::to_string(value);
}

void TracedValue::SetDouble(const char* name, double value) {
  WriteName(name);
  data_ += DoubleToCString(value);
}

void TracedValue::SetBoolean(const char* name, bool value) {
  WriteName(name);
  data_ += value ? "true" : "false";
}

void TracedValue::SetNull(const char* name) {
  WriteName(name);
  data_ += "null";
}

void TracedValue::SetString(const char* name, const char* value) {
  WriteName(name);
  EscapeAndAppendString(value, &data_);
}

void TracedValue::BeginDictionary(const char* name) {
  WriteName(name);
  data_ += '{';
  first_item_ = true;
}

void TracedValue::BeginArray(const char* name) {
  WriteName(name);
  data_ += '[';
  first_item_ = true;
}

void TracedValue::AppendInteger(int value) {
  WriteComma();
  data_ += std::to_string(value);
}

void TracedValue::AppendDouble(double value) {
  WriteComma();
  data_ += DoubleToCString(value);
}

void TracedValue::AppendBoolean(bool value) {
  WriteComma();
  data_ += value ? "true" : "false";
}

void TracedValue::AppendNull() {
  WriteComma();
  data_ += "null";
}

void TracedValue::AppendString(const char* value) {
  WriteComma();
  EscapeAndAppendString(value, &data_);
}

void TracedValue::BeginDictionary() {
  WriteComma();
  data_ += '{';
  first_item_ = true;
}

void TracedValue::BeginArray() {
  WriteComma();
  data_ += '[';
  first_item_ = true;
}

void TracedValue::EndDictionary() {
  data_ += '}';
  first_item_ = false;
}

void TracedValue::EndArray() {
  data_ += ']';
  first_item_ = false;
}

void TracedValue::WriteComma() {
  if (first_item_) {
    first_item_ = false;
  } else {
    data_ += ',';
  }
}

void TracedValue::WriteName(const char* name) {
  WriteComma();
  data_ += '"';
  data_ += name;
  data_ += "\":";
}

void TracedValue::AppendAsTraceFormat(std::string* out) const {
  *out += root_is_array_ ? '[' : '{';
  *out += data_;
  *out += root_is_array_ ? ']' : '}';
}

}  // namespace tracing
}  // namespace node
