// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_PERFETTO_PRODUCER_H_
#define V8_LIBPLATFORM_TRACING_PERFETTO_PRODUCER_H_

#include <atomic>
#include <memory>

#include "perfetto/tracing/core/producer.h"
#include "perfetto/tracing/core/tracing_service.h"
#include "src/base/logging.h"

namespace v8 {
namespace platform {
namespace tracing {

class PerfettoTracingController;

class PerfettoProducer final : public ::perfetto::Producer {
 public:
  using ServiceEndpoint = ::perfetto::TracingService::ProducerEndpoint;

  explicit PerfettoProducer(PerfettoTracingController* tracing_controller);

  ServiceEndpoint* service_endpoint() const { return service_endpoint_.get(); }
  void set_service_endpoint(std::unique_ptr<ServiceEndpoint> endpoint) {
    service_endpoint_ = std::move(endpoint);
  }

  // Create a TraceWriter for the calling thread. The TraceWriter is a
  // thread-local object that writes data into a buffer which is shared between
  // all TraceWriters for a given PerfettoProducer instance. Can only be called
  // after the StartDataSource() callback has been received from the service, as
  // this provides the buffer.
  std::unique_ptr<::perfetto::TraceWriter> CreateTraceWriter() const;

 private:
  // ::perfetto::Producer implementation
  void OnConnect() override;
  void OnDisconnect() override {}
  void OnTracingSetup() override {}
  void SetupDataSource(::perfetto::DataSourceInstanceID,
                       const ::perfetto::DataSourceConfig&) override {}
  void StartDataSource(::perfetto::DataSourceInstanceID,
                       const ::perfetto::DataSourceConfig& cfg) override;
  void StopDataSource(::perfetto::DataSourceInstanceID) override;
  // TODO(petermarshall): Implement Flush(). A final flush happens when the
  // TraceWriter object for each thread is destroyed, but this will be more
  // efficient.
  void Flush(::perfetto::FlushRequestID,
             const ::perfetto::DataSourceInstanceID*, size_t) override {}

  void ClearIncrementalState(
      const ::perfetto::DataSourceInstanceID* data_source_ids,
      size_t num_data_sources) override {
    UNREACHABLE();
  }

  std::unique_ptr<ServiceEndpoint> service_endpoint_;
  uint32_t target_buffer_ = 0;
  PerfettoTracingController* tracing_controller_;
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_PERFETTO_PRODUCER_H_
