// Code adapted from the V8 JSONTraceWriter ...
// Original license header:
//   Copyright 2016 the V8 project authors. All rights reserved.
//   Use of this source code is governed by a BSD-style license that can be
//   found in the LICENSE file.

#ifndef SRC_TRACING_NODE_LDJSON_TRACE_WRITER_H_
#define SRC_TRACING_NODE_LDJSON_TRACE_WRITER_H_

#include "libplatform/v8-tracing.h"
#include "tracing/trace_event_common.h"
#include <sstream>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceObject;
using v8::platform::tracing::TraceWriter;

class LDJSONTraceWriter : public TraceWriter {
 public:
  explicit LDJSONTraceWriter(std::ostream& stream);
  ~LDJSONTraceWriter();
  void AppendTraceEvent(TraceObject* trace_event) override;
  void Flush() override;

  static TraceWriter* Create(std::ostream& stream) {
    return new LDJSONTraceWriter(stream);
  }

 private:
  void AppendArgValue(uint8_t type, TraceObject::ArgValue value);
  void AppendArgValue(v8::ConvertableToTraceFormat*);

  std::ostream& stream_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_NODE_LDJSON_TRACE_WRITER_H_
