// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tracing/traced_value.h"

#if defined(NODE_HAVE_I18N_SUPPORT)
#include <unicode/utf8.h>
#include <unicode/utypes.h>
#endif

#include <sstream>

#include "node_metadata.h"
#include "util.h"

#if defined(_STLP_VENDOR_CSTD)
// STLPort doesn't import fpclassify into the std namespace.
#define FPCLASSIFY_NAMESPACE
#else
#define FPCLASSIFY_NAMESPACE std
#endif

namespace node {
namespace tracing {

namespace {

std::string EscapeString(const char* value) {
  std::string result;
  result += '"';
  char number_buffer[10];
#if defined(NODE_HAVE_I18N_SUPPORT)
  int32_t len = strlen(value);
  int32_t p = 0;
  int32_t i = 0;
  for (; i < len; p = i) {
    UChar32 c;
    U8_NEXT_OR_FFFD(value, i, len, c);
    switch (c) {
      case '\b': result += "\\b"; break;
      case '\f': result += "\\f"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      default:
        if (c < 32 || c > 126) {
          snprintf(
              number_buffer, arraysize(number_buffer), "\\u%04X",
              static_cast<uint16_t>(static_cast<uint16_t>(c)));
          result += number_buffer;
        } else {
          result.append(value + p, i - p);
        }
    }
  }
#else
  // If we do not have ICU, use a modified version of the non-UTF8 aware
  // code from V8's own TracedValue implementation. Note, however, This
  // will not produce correctly serialized results for UTF8 values.
  while (*value) {
    char c = *value++;
    switch (c) {
      case '\b': result += "\\b"; break;
      case '\f': result += "\\f"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      default:
        if (c < '\x20') {
          snprintf(
              number_buffer, arraysize(number_buffer), "\\u%04X",
              static_cast<unsigned>(static_cast<unsigned char>(c)));
          result += number_buffer;
        } else {
          result += c;
        }
    }
  }
#endif  // defined(NODE_HAVE_I18N_SUPPORT)
  result += '"';
  return result;
}

std::string DoubleToCString(double v) {
  switch (FPCLASSIFY_NAMESPACE::fpclassify(v)) {
    case FP_NAN: return "\"NaN\"";
    case FP_INFINITE: return (v < 0.0 ? "\"-Infinity\"" : "\"Infinity\"");
    case FP_ZERO: return "0";
    default:
      // This is a far less sophisticated version than the one used inside v8.
      std::ostringstream stream;
      stream.imbue(std::locale::classic());  // Ignore current locale
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
  data_ += EscapeString(value);
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
  data_ += EscapeString(value);
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

std::unique_ptr<v8::ConvertableToTraceFormat> EnvironmentArgs::Cast() const {
  auto traced_value = tracing::TracedValue::Create();
  traced_value->BeginArray("args");
  for (const std::string& arg : args_) traced_value->AppendString(arg);
  traced_value->EndArray();
  traced_value->BeginArray("exec_args");
  for (const std::string& arg : exec_args_) traced_value->AppendString(arg);
  traced_value->EndArray();
  return traced_value;
}

std::unique_ptr<v8::ConvertableToTraceFormat> AsyncWrapArgs::Cast() const {
  auto data = tracing::TracedValue::Create();
  data->SetInteger("executionAsyncId", execution_async_id_);
  data->SetInteger("triggerAsyncId", trigger_async_id_);
  return data;
}

std::unique_ptr<v8::ConvertableToTraceFormat> ProcessMeta::Cast() const {
  auto trace_process = tracing::TracedValue::Create();
  trace_process->BeginDictionary("versions");
  for (const auto& version : per_process::metadata.versions.pairs()) {
    trace_process->SetString(version.first.data(), version.second.data());
  }
  trace_process->EndDictionary();

  trace_process->SetString("arch", per_process::metadata.arch.c_str());
  trace_process->SetString("platform", per_process::metadata.platform.c_str());

  trace_process->BeginDictionary("release");
  trace_process->SetString("name", per_process::metadata.release.name.c_str());
#if NODE_VERSION_IS_LTS
  trace_process->SetString("lts", per_process::metadata.release.lts.c_str());
#endif
  trace_process->EndDictionary();

  return trace_process;
}

}  // namespace tracing
}  // namespace node
