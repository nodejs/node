// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_PERFETTO_CONSUMER_H_
#define V8_LIBPLATFORM_TRACING_PERFETTO_CONSUMER_H_

#include <memory>

#include "perfetto/tracing/core/consumer.h"
#include "perfetto/tracing/core/tracing_service.h"
#include "src/base/logging.h"

namespace perfetto {
namespace protos {
class ChromeTracePacket;
}  // namespace protos
}  // namespace perfetto

namespace v8 {

namespace base {
class Semaphore;
}

namespace platform {
namespace tracing {

class TraceEventListener;

// A Perfetto Consumer gets streamed trace events from the Service via
// OnTraceData(). A Consumer can be configured (via
// service_endpoint()->EnableTracing()) to listen to various different types of
// trace events. The Consumer is responsible for producing whatever tracing
// output the system should have.

// Implements the V8-specific logic for interacting with the tracing controller
// and directs trace events to the added TraceEventListeners.
class PerfettoConsumer final : public ::perfetto::Consumer {
 public:
  explicit PerfettoConsumer(base::Semaphore* finished);

  using ServiceEndpoint = ::perfetto::TracingService::ConsumerEndpoint;

  // Register a trace event listener that will receive trace events from this
  // consumer. This can be called multiple times to register multiple listeners,
  // but must be called before starting tracing.
  void AddTraceEventListener(TraceEventListener* listener);

  ServiceEndpoint* service_endpoint() const { return service_endpoint_.get(); }
  void set_service_endpoint(std::unique_ptr<ServiceEndpoint> endpoint) {
    service_endpoint_ = std::move(endpoint);
  }

 private:
  // ::perfetto::Consumer implementation
  void OnConnect() override {}
  void OnDisconnect() override {}
  void OnTracingDisabled() override {}
  void OnTraceData(std::vector<::perfetto::TracePacket> packets,
                   bool has_more) override;
  void OnDetach(bool success) override {}
  void OnAttach(bool success, const ::perfetto::TraceConfig&) override {}
  void OnTraceStats(bool success, const ::perfetto::TraceStats&) override {
    UNREACHABLE();
  }
  void OnObservableEvents(const ::perfetto::ObservableEvents&) override {
    UNREACHABLE();
  }

  std::unique_ptr<ServiceEndpoint> service_endpoint_;
  base::Semaphore* finished_semaphore_;
  std::vector<TraceEventListener*> listeners_;
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_PERFETTO_CONSUMER_H_
