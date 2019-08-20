// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_JSON_TRACE_EVENT_LISTENER_H_
#define V8_LIBPLATFORM_TRACING_JSON_TRACE_EVENT_LISTENER_H_

#include <ostream>

#include "src/libplatform/tracing/trace-event-listener.h"

namespace perfetto {
namespace protos {
class ChromeTraceEvent_Arg;
}  // namespace protos
}  // namespace perfetto

namespace v8 {
namespace platform {
namespace tracing {

// A listener that converts the proto trace data to JSON and writes it to a
// file.
class JSONTraceEventListener final : public TraceEventListener {
 public:
  explicit JSONTraceEventListener(std::ostream* stream);
  ~JSONTraceEventListener() override;

  void ProcessPacket(const ::perfetto::protos::TracePacket& packet) override;

 private:
  // Internal implementation
  void AppendJSONString(const char* str);
  void AppendArgValue(const ::perfetto::protos::ChromeTraceEvent_Arg& arg);

  std::ostream* stream_;
  bool append_comma_ = false;
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_JSON_TRACE_EVENT_LISTENER_H_
