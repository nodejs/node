// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/perfetto-producer.h"

#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/trace_writer.h"
#include "src/libplatform/tracing/perfetto-tasks.h"
#include "src/libplatform/tracing/perfetto-tracing-controller.h"

namespace v8 {
namespace platform {
namespace tracing {

void PerfettoProducer::OnConnect() {
  ::perfetto::DataSourceDescriptor ds_desc;
  ds_desc.set_name("v8.trace_events");
  service_endpoint_->RegisterDataSource(ds_desc);
}

void PerfettoProducer::StartDataSource(
    ::perfetto::DataSourceInstanceID, const ::perfetto::DataSourceConfig& cfg) {
  target_buffer_ = cfg.target_buffer();
  tracing_controller_->OnProducerReady();
}

void PerfettoProducer::StopDataSource(::perfetto::DataSourceInstanceID) {
  target_buffer_ = 0;
}

std::unique_ptr<::perfetto::TraceWriter> PerfettoProducer::CreateTraceWriter()
    const {
  CHECK_NE(0, target_buffer_);
  return service_endpoint_->CreateTraceWriter(target_buffer_);
}

PerfettoProducer::PerfettoProducer(
    PerfettoTracingController* tracing_controller)
    : tracing_controller_(tracing_controller) {}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
