// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/json-trace-event-listener.h"

#include <cmath>

#include "base/trace_event/common/trace_event_common.h"
#include "perfetto/tracing.h"
#include "protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"
#include "protos/perfetto/trace/trace.pb.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace platform {
namespace tracing {

JSONTraceEventListener::JSONTraceEventListener(std::ostream* stream)
    : stream_(stream) {
  *stream_ << "{\"traceEvents\":[";
}

JSONTraceEventListener::~JSONTraceEventListener() { *stream_ << "]}"; }

// TODO(petermarshall): Clean up this code which was copied from trace-writer.cc
// once we've removed that file.

// Writes the given string, taking care to escape characters when necessary.
void JSONTraceEventListener::AppendJSONString(const char* str) {
  size_t len = strlen(str);
  *stream_ << "\"";
  for (size_t i = 0; i < len; ++i) {
    // All of the permitted escape sequences in JSON strings, as per
    // https://mathiasbynens.be/notes/javascript-escapes
    switch (str[i]) {
      case '\b':
        *stream_ << "\\b";
        break;
      case '\f':
        *stream_ << "\\f";
        break;
      case '\n':
        *stream_ << "\\n";
        break;
      case '\r':
        *stream_ << "\\r";
        break;
      case '\t':
        *stream_ << "\\t";
        break;
      case '\"':
        *stream_ << "\\\"";
        break;
      case '\\':
        *stream_ << "\\\\";
        break;
      // Note that because we use double quotes for JSON strings,
      // we don't need to escape single quotes.
      default:
        *stream_ << str[i];
        break;
    }
  }
  *stream_ << "\"";
}

void JSONTraceEventListener::AppendArgValue(
    const ::perfetto::protos::ChromeTraceEvent_Arg& arg) {
  if (arg.has_bool_value()) {
    *stream_ << (arg.bool_value() ? "true" : "false");
  } else if (arg.has_uint_value()) {
    *stream_ << arg.uint_value();
  } else if (arg.has_int_value()) {
    *stream_ << arg.int_value();
  } else if (arg.has_double_value()) {
    std::string real;
    double val = arg.double_value();
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
    *stream_ << real;
  } else if (arg.has_string_value()) {
    AppendJSONString(arg.string_value().c_str());
  } else if (arg.has_pointer_value()) {
    // JSON only supports double and int numbers.
    // So as not to lose bits from a 64-bit pointer, output as a hex string.
    *stream_ << "\"0x" << std::hex << arg.pointer_value() << std::dec << "\"";
  } else if (arg.has_json_value()) {
    *stream_ << arg.json_value();
  }
  // V8 does not emit proto arguments currently.
  CHECK(!arg.has_traced_value());
}

void JSONTraceEventListener::ProcessPacket(
    const ::perfetto::protos::TracePacket& packet) {
  for (const ::perfetto::protos::ChromeTraceEvent& event :
       packet.chrome_events().trace_events()) {
    if (append_comma_) *stream_ << ",";
    append_comma_ = true;

    // TODO(petermarshall): Handle int64 fields differently?
    // clang-format off
    *stream_ << "{\"pid\":" << event.process_id()
            << ",\"tid\":" << event.thread_id()
            << ",\"ts\":" << event.timestamp()
            << ",\"tts\":" << event.thread_timestamp()
            << ",\"ph\":\"" << static_cast<char>(event.phase())
            << "\",\"cat\":\"" << event.category_group_name()
            << "\",\"name\":\"" << event.name()
            << "\",\"dur\":" << event.duration()
            << ",\"tdur\":" << event.thread_duration();
    // clang-format on

    if (event.flags() &
        (TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT)) {
      *stream_ << ",\"bind_id\":\"0x" << std::hex << event.bind_id() << "\""
               << std::dec;
      if (event.flags() & TRACE_EVENT_FLAG_FLOW_IN) {
        *stream_ << ",\"flow_in\":true";
      }
      if (event.flags() & TRACE_EVENT_FLAG_FLOW_OUT) {
        *stream_ << ",\"flow_out\":true";
      }
    }
    if (event.flags() & TRACE_EVENT_FLAG_HAS_ID) {
      if (event.has_scope()) {
        *stream_ << ",\"scope\":\"" << event.scope() << "\"";
      }
      // So as not to lose bits from a 64-bit integer, output as a hex string.
      *stream_ << ",\"id\":\"0x" << std::hex << event.id() << "\"" << std::dec;
    }

    *stream_ << ",\"args\":{";

    int i = 0;
    for (const ::perfetto::protos::ChromeTraceEvent_Arg& arg : event.args()) {
      if (i++ > 0) *stream_ << ",";
      *stream_ << "\"" << arg.name() << "\":";
      AppendArgValue(arg);
    }
    *stream_ << "}}";
  }
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
