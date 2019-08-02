// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/perfetto-consumer.h"

#include "perfetto/trace/chrome/chrome_trace_packet.pb.h"
#include "perfetto/tracing/core/trace_packet.h"
#include "src/base/macros.h"
#include "src/base/platform/semaphore.h"
#include "src/libplatform/tracing/trace-event-listener.h"

namespace v8 {
namespace platform {
namespace tracing {

PerfettoConsumer::PerfettoConsumer(base::Semaphore* finished)
    : finished_semaphore_(finished) {}

void PerfettoConsumer::OnTraceData(std::vector<::perfetto::TracePacket> packets,
                                   bool has_more) {
  for (const ::perfetto::TracePacket& packet : packets) {
    perfetto::protos::ChromeTracePacket proto_packet;
    bool success = packet.Decode(&proto_packet);
    USE(success);
    DCHECK(success);

    for (TraceEventListener* listener : listeners_) {
      listener->ProcessPacket(proto_packet);
    }
  }
  // PerfettoTracingController::StopTracing() waits on this sempahore. This is
  // so that we can ensure that this consumer has finished consuming all of the
  // trace events from the buffer before the buffer is destroyed.
  if (!has_more) finished_semaphore_->Signal();
}

void PerfettoConsumer::AddTraceEventListener(TraceEventListener* listener) {
  listeners_.push_back(listener);
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
