// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/trace-writer.h"

#include <cmath>

#include "base/trace_event/common/trace_event_common.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace platform {
namespace tracing {

// Currently we do not support JSON-escaping strings in trace arguments.
// Thus we perform an IsJSONString() check before writing any string argument.
// In particular, this means strings cannot have control characters or \.
V8_INLINE static bool IsJSONString(const char* str) {
  size_t len = strlen(str);
  for (size_t i = 0; i < len; ++i) {
    if (iscntrl(str[i]) || str[i] == '\\') {
      return false;
    }
  }
  return true;
}

void JSONTraceWriter::AppendArgValue(uint8_t type,
                                     TraceObject::ArgValue value) {
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      stream_ << (value.as_bool ? "true" : "false");
      break;
    case TRACE_VALUE_TYPE_UINT:
      stream_ << value.as_uint;
      break;
    case TRACE_VALUE_TYPE_INT:
      stream_ << value.as_int;
      break;
    case TRACE_VALUE_TYPE_DOUBLE: {
      std::string real;
      double val = value.as_double;
      if (std::isfinite(val)) {
        std::ostringstream convert_stream;
        convert_stream << val;
        real = convert_stream.str();
        // Ensure that the number has a .0 if there's no decimal or 'e'.  This
        // makes sure that when we read the JSON back, it's interpreted as a
        // real rather than an int.
        if (real.find('.') == std::string::npos &&
            real.find('e') == std::string::npos &&
            real.find('E') == std::string::npos) {
          real += ".0";
        }
      } else if (std::isnan(val)) {
        // The JSON spec doesn't allow NaN and Infinity (since these are
        // objects in EcmaScript).  Use strings instead.
        real = "\"NaN\"";
      } else if (val < 0) {
        real = "\"-Infinity\"";
      } else {
        real = "\"Infinity\"";
      }
      stream_ << real;
      break;
    }
    case TRACE_VALUE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      stream_ << "\"" << value.as_pointer << "\"";
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      // Strings are currently not JSON-escaped, so we need to perform a check
      // to see if they are valid JSON strings.
      CHECK(value.as_string != nullptr && IsJSONString(value.as_string));
      stream_ << (value.as_string ? value.as_string : "NULL");
      break;
    default:
      UNREACHABLE();
      break;
  }
}

JSONTraceWriter::JSONTraceWriter(std::ostream& stream) : stream_(stream) {
  stream_ << "{\"traceEvents\":[";
}

JSONTraceWriter::~JSONTraceWriter() { stream_ << "]}"; }

void JSONTraceWriter::AppendTraceEvent(TraceObject* trace_event) {
  if (append_comma_) stream_ << ",";
  append_comma_ = true;
  stream_ << "{\"pid\":" << trace_event->pid()
          << ",\"tid\":" << trace_event->tid()
          << ",\"ts\":" << trace_event->ts()
          << ",\"tts\":" << trace_event->tts() << ",\"ph\":\""
          << trace_event->phase() << "\",\"cat\":\""
          << TracingController::GetCategoryGroupName(
                 trace_event->category_enabled_flag())
          << "\",\"name\":\"" << trace_event->name()
          << "\",\"dur\":" << trace_event->duration()
          << ",\"tdur\":" << trace_event->cpu_duration();
  if (trace_event->flags() & TRACE_EVENT_FLAG_HAS_ID) {
    if (trace_event->scope() != nullptr) {
      stream_ << ",\"scope\":\"" << trace_event->scope() << "\"";
    }
    // So as not to lose bits from a 64-bit integer, output as a hex string.
    stream_ << ",\"id\":\"0x" << std::hex << trace_event->id() << "\""
            << std::dec;
  }
  stream_ << ",\"args\":{";
  const char** arg_names = trace_event->arg_names();
  const uint8_t* arg_types = trace_event->arg_types();
  TraceObject::ArgValue* arg_values = trace_event->arg_values();
  for (int i = 0; i < trace_event->num_args(); ++i) {
    if (i > 0) stream_ << ",";
    stream_ << "\"" << arg_names[i] << "\":";
    AppendArgValue(arg_types[i], arg_values[i]);
  }
  stream_ << "}}";
  // TODO(fmeawad): Add support for Flow Events.
}

void JSONTraceWriter::Flush() {}

TraceWriter* TraceWriter::CreateJSONTraceWriter(std::ostream& stream) {
  return new JSONTraceWriter(stream);
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
