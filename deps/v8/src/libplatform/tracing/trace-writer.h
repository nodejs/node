// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_TRACE_WRITER_H_
#define V8_LIBPLATFORM_TRACING_TRACE_WRITER_H_

#include "include/libplatform/v8-tracing.h"

namespace v8 {
namespace platform {
namespace tracing {

class Recorder;

class JSONTraceWriter : public TraceWriter {
 public:
  explicit JSONTraceWriter(std::ostream& stream);
  JSONTraceWriter(std::ostream& stream, const std::string& tag);
  ~JSONTraceWriter() override;
  void AppendTraceEvent(TraceObject* trace_event) override;
  void Flush() override;

 private:
  void AppendArgValue(uint8_t type, TraceObject::ArgValue value);
  void AppendArgValue(v8::ConvertableToTraceFormat*);

  std::ostream& stream_;
  bool append_comma_ = false;
};

#if defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
class SystemInstrumentationTraceWriter : public TraceWriter {
 public:
  SystemInstrumentationTraceWriter();
  ~SystemInstrumentationTraceWriter() override;
  void AppendTraceEvent(TraceObject* trace_event) override;
  void Flush() override;

 private:
  std::unique_ptr<Recorder> recorder_;
};
#endif

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_TRACE_WRITER_H_
