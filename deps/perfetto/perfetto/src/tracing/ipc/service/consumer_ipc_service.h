/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SRC_TRACING_IPC_SERVICE_CONSUMER_IPC_SERVICE_H_
#define SRC_TRACING_IPC_SERVICE_CONSUMER_IPC_SERVICE_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/ipc/basic_types.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/forward_decls.h"
#include "protos/perfetto/ipc/consumer_port.ipc.h"

namespace perfetto {

namespace ipc {
class Host;
}  // namespace ipc

// Implements the Consumer port of the IPC service. This class proxies requests
// and responses between the core service logic (|svc_|) and remote Consumer(s)
// on the IPC socket, through the methods overriddden from ConsumerPort.
class ConsumerIPCService : public protos::gen::ConsumerPort {
 public:
  explicit ConsumerIPCService(TracingService* core_service);
  ~ConsumerIPCService() override;

  // ConsumerPort implementation (from .proto IPC definition).
  void EnableTracing(const protos::gen::EnableTracingRequest&,
                     DeferredEnableTracingResponse) override;
  void StartTracing(const protos::gen::StartTracingRequest&,
                    DeferredStartTracingResponse) override;
  void ChangeTraceConfig(const protos::gen::ChangeTraceConfigRequest&,
                         DeferredChangeTraceConfigResponse) override;
  void DisableTracing(const protos::gen::DisableTracingRequest&,
                      DeferredDisableTracingResponse) override;
  void ReadBuffers(const protos::gen::ReadBuffersRequest&,
                   DeferredReadBuffersResponse) override;
  void FreeBuffers(const protos::gen::FreeBuffersRequest&,
                   DeferredFreeBuffersResponse) override;
  void Flush(const protos::gen::FlushRequest&, DeferredFlushResponse) override;
  void Detach(const protos::gen::DetachRequest&,
              DeferredDetachResponse) override;
  void Attach(const protos::gen::AttachRequest&,
              DeferredAttachResponse) override;
  void GetTraceStats(const protos::gen::GetTraceStatsRequest&,
                     DeferredGetTraceStatsResponse) override;
  void ObserveEvents(const protos::gen::ObserveEventsRequest&,
                     DeferredObserveEventsResponse) override;
  void QueryServiceState(const protos::gen::QueryServiceStateRequest&,
                         DeferredQueryServiceStateResponse) override;
  void QueryCapabilities(const protos::gen::QueryCapabilitiesRequest&,
                         DeferredQueryCapabilitiesResponse) override;
  void OnClientDisconnected() override;

 private:
  // Acts like a Consumer with the core Service business logic (which doesn't
  // know anything about the remote transport), but all it does is proxying
  // methods to the remote Consumer on the other side of the IPC channel.
  class RemoteConsumer : public Consumer {
   public:
    RemoteConsumer();
    ~RemoteConsumer() override;

    // These methods are called by the |core_service_| business logic. There is
    // no connection here, these methods are posted straight away.
    void OnConnect() override;
    void OnDisconnect() override;
    void OnTracingDisabled() override;
    void OnTraceData(std::vector<TracePacket>, bool has_more) override;
    void OnDetach(bool) override;
    void OnAttach(bool, const TraceConfig&) override;
    void OnTraceStats(bool, const TraceStats&) override;
    void OnObservableEvents(const ObservableEvents&) override;

    void CloseObserveEventsResponseStream();

    // The interface obtained from the core service business logic through
    // TracingService::ConnectConsumer(this). This allows to invoke methods for
    // a specific Consumer on the Service business logic.
    std::unique_ptr<TracingService::ConsumerEndpoint> service_endpoint;

    // After ReadBuffers() is invoked, this binds the async callback that
    // allows to stream trace packets back to the client.
    DeferredReadBuffersResponse read_buffers_response;

    // After EnableTracing() is invoked, this binds the async callback that
    // allows to send the OnTracingDisabled notification.
    DeferredEnableTracingResponse enable_tracing_response;

    // After Detach() is invoked, this binds the async callback that allows to
    // send the session id to the consumer.
    DeferredDetachResponse detach_response;

    // As above, but for the Attach() case.
    DeferredAttachResponse attach_response;

    // As above, but for GetTraceStats().
    DeferredGetTraceStatsResponse get_trace_stats_response;

    // After ObserveEvents() is invoked, this binds the async callback that
    // allows to stream ObservableEvents back to the client.
    DeferredObserveEventsResponse observe_events_response;
  };

  // This has to be a container that doesn't invalidate iterators.
  using PendingFlushResponses = std::list<DeferredFlushResponse>;
  using PendingQuerySvcResponses = std::list<DeferredQueryServiceStateResponse>;
  using PendingQueryCapabilitiesResponses =
      std::list<DeferredQueryCapabilitiesResponse>;

  ConsumerIPCService(const ConsumerIPCService&) = delete;
  ConsumerIPCService& operator=(const ConsumerIPCService&) = delete;

  // Returns the ConsumerEndpoint in the core business logic that corresponds to
  // the current IPC request.
  RemoteConsumer* GetConsumerForCurrentRequest();

  void OnFlushCallback(bool success, PendingFlushResponses::iterator);
  void OnQueryServiceCallback(bool success,
                              const TracingServiceState&,
                              PendingQuerySvcResponses::iterator);
  void OnQueryCapabilitiesCallback(const TracingServiceCapabilities&,
                                   PendingQueryCapabilitiesResponses::iterator);

  TracingService* const core_service_;

  // Maps IPC clients to ConsumerEndpoint instances registered on the
  // |core_service_| business logic.
  std::map<ipc::ClientID, std::unique_ptr<RemoteConsumer>> consumers_;

  PendingFlushResponses pending_flush_responses_;
  PendingQuerySvcResponses pending_query_service_responses_;
  PendingQueryCapabilitiesResponses pending_query_capabilities_responses_;

  base::WeakPtrFactory<ConsumerIPCService> weak_ptr_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACING_IPC_SERVICE_CONSUMER_IPC_SERVICE_H_
