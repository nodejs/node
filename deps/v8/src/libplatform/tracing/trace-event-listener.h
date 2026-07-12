// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_TRACE_EVENT_LISTENER_H_
#define V8_LIBPLATFORM_TRACING_TRACE_EVENT_LISTENER_H_

#include <vector>

#include "libplatform/libplatform-export.h"

namespace v8 {
namespace platform {
namespace tracing {

// A TraceEventListener is a simple interface that allows subclasses to listen
// to trace events. This interface is to hide the more complex interactions that
// the PerfettoConsumer class has to perform. Clients override ParseFromArray()
// to process traces, e.g. to write them to a file as JSON or for testing
// purposes.
class V8_PLATFORM_EXPORT TraceEventListener {
 public:
  virtual ~TraceEventListener() = default;
  virtual void ParseFromArray(const std::vector<char>& array) = 0;
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_TRACE_EVENT_LISTENER_H_
