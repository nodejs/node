/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACING_TRACE_WRITER_BASE_H_
#define INCLUDE_PERFETTO_TRACING_TRACE_WRITER_BASE_H_

#include "perfetto/protozero/message_handle.h"

namespace perfetto {

namespace protos {
namespace pbzero {
class TracePacket;
}  // namespace pbzero
}  // namespace protos

// The bare-minimum subset of the TraceWriter interface that is exposed as a
// fully public API.
// See comments in /include/perfetto/ext/tracing/core/trace_writer.h.
class TraceWriterBase {
 public:
  virtual ~TraceWriterBase();

  virtual protozero::MessageHandle<protos::pbzero::TracePacket>
  NewTracePacket() = 0;

  virtual void Flush(std::function<void()> callback = {}) = 0;
  virtual uint64_t written() const = 0;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_TRACE_WRITER_BASE_H_
