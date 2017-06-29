// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_LIBPLATFORM_TRACING_TRACE_WRITER_H_
#define SRC_LIBPLATFORM_TRACING_TRACE_WRITER_H_

#include "include/libplatform/v8-tracing.h"

namespace v8 {
namespace platform {
namespace tracing {

class JSONTraceWriter : public TraceWriter {
 public:
  explicit JSONTraceWriter(std::ostream& stream);
  ~JSONTraceWriter();
  void AppendTraceEvent(TraceObject* trace_event) override;
  void Flush() override;

 private:
  void AppendArgValue(uint8_t type, TraceObject::ArgValue value);
  void AppendArgValue(v8::ConvertableToTraceFormat*);

  std::ostream& stream_;
  bool append_comma_ = false;
};

class NullTraceWriter : public TraceWriter {
 public:
  NullTraceWriter() = default;
  ~NullTraceWriter() = default;
  void AppendTraceEvent(TraceObject*) override {}
  void Flush() override {}
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // SRC_LIBPLATFORM_TRACING_TRACE_WRITER_H_
