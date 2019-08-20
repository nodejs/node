// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_TRACE_EVENT_LISTENER_H_
#define V8_LIBPLATFORM_TRACING_TRACE_EVENT_LISTENER_H_

#include <vector>

namespace perfetto {
namespace protos {
class TracePacket;
}  // namespace protos
}  // namespace perfetto

namespace v8 {
namespace platform {
namespace tracing {

// A TraceEventListener is a simple interface that allows subclasses to listen
// to trace events. This interface is to hide the more complex interactions that
// the PerfettoConsumer class has to perform. Clients override ProcessPacket()
// to respond to trace events, e.g. to write them to a file as JSON or for
// testing purposes.
class TraceEventListener {
 public:
  virtual ~TraceEventListener() = default;
  virtual void ProcessPacket(const ::perfetto::protos::TracePacket& packet) = 0;

  void ParseFromArray(const std::vector<char>& array);
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_TRACE_EVENT_LISTENER_H_
