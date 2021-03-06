// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/trace-event-listener.h"

#include "protos/perfetto/trace/trace.pb.h"
#include "src/base/logging.h"

namespace v8 {
namespace platform {
namespace tracing {

void TraceEventListener::ParseFromArray(const std::vector<char>& array) {
  perfetto::protos::Trace trace;
  CHECK(trace.ParseFromArray(array.data(), static_cast<int>(array.size())));

  for (int i = 0; i < trace.packet_size(); i++) {
    // TODO(petermarshall): ChromeTracePacket instead.
    const perfetto::protos::TracePacket& packet = trace.packet(i);
    ProcessPacket(packet);
  }
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
