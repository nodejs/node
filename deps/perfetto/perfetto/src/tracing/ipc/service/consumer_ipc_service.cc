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

#include "src/tracing/ipc/service/consumer_ipc_service.h"

#include <inttypes.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/ipc/basic_types.h"
#include "perfetto/ext/ipc/host.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"
#include "perfetto/ext/tracing/core/slice.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_stats.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/core/tracing_service_capabilities.h"
#include "perfetto/tracing/core/tracing_service_state.h"

namespace perfetto {

ConsumerIPCService::ConsumerIPCService(TracingService* core_service)
    : core_service_(core_service), weak_ptr_factory_(this) {}

ConsumerIPCService::~ConsumerIPCService() = default;

ConsumerIPCService::RemoteConsumer*
ConsumerIPCService::GetConsumerForCurrentRequest() {
  const ipc::ClientID ipc_client_id = ipc::Service::client_info().client_id();
  const uid_t uid = ipc::Service::client_info().uid();
  PERFETTO_CHECK(ipc_client_id);
  auto it = consumers_.find(ipc_client_id);
  if (it == consumers_.end()) {
    auto* remote_consumer = new RemoteConsumer();
    consumers_[ipc_client_id].reset(remote_consumer);
    remote_consumer->service_endpoint =
        core_service_->ConnectConsumer(remote_consumer, uid);
    return remote_consumer;
  }
  return it->second.get();
}

// Called by the IPC layer.
void ConsumerIPCService::OnClientDisconnected() {
  ipc::ClientID client_id = ipc::Service::client_info().client_id();
  consumers_.erase(client_id);
}

// Called by the IPC layer.
void ConsumerIPCService::EnableTracing(
    const protos::gen::EnableTracingRequest& req,
    DeferredEnableTracingResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  if (req.attach_notification_only()) {
    remote_consumer->enable_tracing_response = std::move(resp);
    return;
  }
  const TraceConfig& trace_config = req.trace_config();
  base::ScopedFile fd;
  if (trace_config.write_into_file() && trace_config.output_path().empty())
    fd = ipc::Service::TakeReceivedFD();
  remote_consumer->service_endpoint->EnableTracing(trace_config, std::move(fd));
  remote_consumer->enable_tracing_response = std::move(resp);
}

// Called by the IPC layer.
void ConsumerIPCService::StartTracing(const protos::gen::StartTracingRequest&,
                                      DeferredStartTracingResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  remote_consumer->service_endpoint->StartTracing();
  resp.Resolve(ipc::AsyncResult<protos::gen::StartTracingResponse>::Create());
}

// Called by the IPC layer.
void ConsumerIPCService::ChangeTraceConfig(
    const protos::gen::ChangeTraceConfigRequest& req,
    DeferredChangeTraceConfigResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  remote_consumer->service_endpoint->ChangeTraceConfig(req.trace_config());
  resp.Resolve(
      ipc::AsyncResult<protos::gen::ChangeTraceConfigResponse>::Create());
}

// Called by the IPC layer.
void ConsumerIPCService::DisableTracing(
    const protos::gen::DisableTracingRequest&,
    DeferredDisableTracingResponse resp) {
  GetConsumerForCurrentRequest()->service_endpoint->DisableTracing();
  resp.Resolve(ipc::AsyncResult<protos::gen::DisableTracingResponse>::Create());
}

// Called by the IPC layer.
void ConsumerIPCService::ReadBuffers(const protos::gen::ReadBuffersRequest&,
                                     DeferredReadBuffersResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  remote_consumer->read_buffers_response = std::move(resp);
  remote_consumer->service_endpoint->ReadBuffers();
}

// Called by the IPC layer.
void ConsumerIPCService::FreeBuffers(const protos::gen::FreeBuffersRequest&,
                                     DeferredFreeBuffersResponse resp) {
  GetConsumerForCurrentRequest()->service_endpoint->FreeBuffers();
  resp.Resolve(ipc::AsyncResult<protos::gen::FreeBuffersResponse>::Create());
}

// Called by the IPC layer.
void ConsumerIPCService::Flush(const protos::gen::FlushRequest& req,
                               DeferredFlushResponse resp) {
  auto it = pending_flush_responses_.insert(pending_flush_responses_.end(),
                                            std::move(resp));
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto callback = [weak_this, it](bool success) {
    if (weak_this)
      weak_this->OnFlushCallback(success, std::move(it));
  };
  GetConsumerForCurrentRequest()->service_endpoint->Flush(req.timeout_ms(),
                                                          std::move(callback));
}

// Called by the IPC layer.
void ConsumerIPCService::Detach(const protos::gen::DetachRequest& req,
                                DeferredDetachResponse resp) {
  // OnDetach() will resolve the |detach_response|.
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  remote_consumer->detach_response = std::move(resp);
  remote_consumer->service_endpoint->Detach(req.key());
}

// Called by the IPC layer.
void ConsumerIPCService::Attach(const protos::gen::AttachRequest& req,
                                DeferredAttachResponse resp) {
  // OnAttach() will resolve the |attach_response|.
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  remote_consumer->attach_response = std::move(resp);
  remote_consumer->service_endpoint->Attach(req.key());
}

// Called by the IPC layer.
void ConsumerIPCService::GetTraceStats(const protos::gen::GetTraceStatsRequest&,
                                       DeferredGetTraceStatsResponse resp) {
  // OnTraceStats() will resolve the |get_trace_stats_response|.
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  remote_consumer->get_trace_stats_response = std::move(resp);
  remote_consumer->service_endpoint->GetTraceStats();
}

// Called by the IPC layer.
void ConsumerIPCService::ObserveEvents(
    const protos::gen::ObserveEventsRequest& req,
    DeferredObserveEventsResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();

  // If there's a prior stream, close it so that client can clean it up.
  remote_consumer->CloseObserveEventsResponseStream();

  remote_consumer->observe_events_response = std::move(resp);

  uint32_t events_mask = 0;
  for (const auto& type : req.events_to_observe()) {
    events_mask |= static_cast<uint32_t>(type);
  }
  remote_consumer->service_endpoint->ObserveEvents(events_mask);

  // If no events are to be observed, close the stream immediately so that the
  // client can clean up.
  if (events_mask == 0)
    remote_consumer->CloseObserveEventsResponseStream();
}

// Called by the IPC layer.
void ConsumerIPCService::QueryServiceState(
    const protos::gen::QueryServiceStateRequest&,
    DeferredQueryServiceStateResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  auto it = pending_query_service_responses_.insert(
      pending_query_service_responses_.end(), std::move(resp));
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto callback = [weak_this, it](bool success,
                                  const TracingServiceState& svc_state) {
    if (weak_this)
      weak_this->OnQueryServiceCallback(success, svc_state, std::move(it));
  };
  remote_consumer->service_endpoint->QueryServiceState(callback);
}

// Called by the service in response to service_endpoint->QueryServiceState().
void ConsumerIPCService::OnQueryServiceCallback(
    bool success,
    const TracingServiceState& svc_state,
    PendingQuerySvcResponses::iterator pending_response_it) {
  DeferredQueryServiceStateResponse response(std::move(*pending_response_it));
  pending_query_service_responses_.erase(pending_response_it);
  if (!success) {
    response.Reject();
    return;
  }

  // The TracingServiceState object might be too big to fit into a single IPC
  // message because it contains the DataSourceDescriptor of each data source.
  // Here we split it in chunks to fit in the IPC limit, observing the
  // following rule: each chunk must be invididually a valid TracingServiceState
  // message; all the chunks concatenated together must form the original
  // message. This is to deal with the legacy API that was just sending one
  // whole message (failing in presence of too many data sources, b/153142114).
  // The message is split as follows: we take the whole TracingServiceState,
  // take out the data sources section (which is a top-level repeated field)
  // and re-add them one-by-one. If, in the process of appending, the IPC msg
  // size is reached, a new chunk is created. This assumes that the rest of
  // TracingServiceState fits in one IPC message and each DataSourceDescriptor
  // fits in the worst case in a dedicated message (which is true, because
  // otherwise the RegisterDataSource() which passes the descriptor in the first
  // place would fail).

  std::vector<uint8_t> chunked_reply;

  // Transmits the current chunk and starts a new one.
  bool sent_eof = false;
  auto send_chunked_reply = [&chunked_reply, &response,
                             &sent_eof](bool has_more) {
    PERFETTO_CHECK(!sent_eof);
    sent_eof = !has_more;
    auto resp =
        ipc::AsyncResult<protos::gen::QueryServiceStateResponse>::Create();
    resp.set_has_more(has_more);
    PERFETTO_CHECK(resp->mutable_service_state()->ParseFromArray(
        chunked_reply.data(), chunked_reply.size()));
    chunked_reply.clear();
    response.Resolve(std::move(resp));
  };

  // Create a copy of the whole response and cut away the data_sources section.
  protos::gen::TracingServiceState svc_state_copy = svc_state;
  auto data_sources = std::move(*svc_state_copy.mutable_data_sources());
  chunked_reply = svc_state_copy.SerializeAsArray();

  // Now re-add them fitting within the IPC message limits (- some margin for
  // the outer IPC frame).
  constexpr size_t kMaxMsgSize = ipc::kIPCBufferSize - 128;
  for (const auto& data_source : data_sources) {
    protos::gen::TracingServiceState tmp;
    tmp.mutable_data_sources()->emplace_back(std::move(data_source));
    std::vector<uint8_t> chunk = tmp.SerializeAsArray();
    if (chunked_reply.size() + chunk.size() < kMaxMsgSize) {
      chunked_reply.insert(chunked_reply.end(), chunk.begin(), chunk.end());
    } else {
      send_chunked_reply(/*has_more=*/true);
      chunked_reply = std::move(chunk);
    }
  }

  PERFETTO_DCHECK(!chunked_reply.empty());
  send_chunked_reply(/*has_more=*/false);
  PERFETTO_CHECK(sent_eof);
}

// Called by the service in response to a service_endpoint->Flush() request.
void ConsumerIPCService::OnFlushCallback(
    bool success,
    PendingFlushResponses::iterator pending_response_it) {
  DeferredFlushResponse response(std::move(*pending_response_it));
  pending_flush_responses_.erase(pending_response_it);
  if (success) {
    response.Resolve(ipc::AsyncResult<protos::gen::FlushResponse>::Create());
  } else {
    response.Reject();
  }
}

void ConsumerIPCService::QueryCapabilities(
    const protos::gen::QueryCapabilitiesRequest&,
    DeferredQueryCapabilitiesResponse resp) {
  RemoteConsumer* remote_consumer = GetConsumerForCurrentRequest();
  auto it = pending_query_capabilities_responses_.insert(
      pending_query_capabilities_responses_.end(), std::move(resp));
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto callback = [weak_this, it](const TracingServiceCapabilities& caps) {
    if (weak_this)
      weak_this->OnQueryCapabilitiesCallback(caps, std::move(it));
  };
  remote_consumer->service_endpoint->QueryCapabilities(callback);
}

// Called by the service in response to service_endpoint->QueryCapabilities().
void ConsumerIPCService::OnQueryCapabilitiesCallback(
    const TracingServiceCapabilities& caps,
    PendingQueryCapabilitiesResponses::iterator pending_response_it) {
  DeferredQueryCapabilitiesResponse response(std::move(*pending_response_it));
  pending_query_capabilities_responses_.erase(pending_response_it);
  auto resp =
      ipc::AsyncResult<protos::gen::QueryCapabilitiesResponse>::Create();
  *resp->mutable_capabilities() = caps;
  response.Resolve(std::move(resp));
}

////////////////////////////////////////////////////////////////////////////////
// RemoteConsumer methods
////////////////////////////////////////////////////////////////////////////////

ConsumerIPCService::RemoteConsumer::RemoteConsumer() = default;
ConsumerIPCService::RemoteConsumer::~RemoteConsumer() = default;

// Invoked by the |core_service_| business logic after the ConnectConsumer()
// call. There is nothing to do here, we really expected the ConnectConsumer()
// to just work in the local case.
void ConsumerIPCService::RemoteConsumer::OnConnect() {}

// Invoked by the |core_service_| business logic after we destroy the
// |service_endpoint| (in the RemoteConsumer dtor).
void ConsumerIPCService::RemoteConsumer::OnDisconnect() {}

void ConsumerIPCService::RemoteConsumer::OnTracingDisabled() {
  if (enable_tracing_response.IsBound()) {
    auto result =
        ipc::AsyncResult<protos::gen::EnableTracingResponse>::Create();
    result->set_disabled(true);
    enable_tracing_response.Resolve(std::move(result));
  }
}

void ConsumerIPCService::RemoteConsumer::OnTraceData(
    std::vector<TracePacket> trace_packets,
    bool has_more) {
  if (!read_buffers_response.IsBound())
    return;

  auto result = ipc::AsyncResult<protos::gen::ReadBuffersResponse>::Create();

  // A TracePacket might be too big to fit into a single IPC message (max
  // kIPCBufferSize). However a TracePacket is made of slices and each slice
  // is way smaller than kIPCBufferSize (a slice size is effectively bounded by
  // the max chunk size of the SharedMemoryABI). When sending a TracePacket,
  // if its slices don't fit within one IPC, chunk them over several contiguous
  // IPCs using the |last_slice_for_packet| for glueing on the other side.
  static_assert(ipc::kIPCBufferSize >= SharedMemoryABI::kMaxPageSize * 2,
                "kIPCBufferSize too small given the max possible slice size");

  auto send_ipc_reply = [this, &result](bool more) {
    result.set_has_more(more);
    read_buffers_response.Resolve(std::move(result));
    result = ipc::AsyncResult<protos::gen::ReadBuffersResponse>::Create();
  };

  size_t approx_reply_size = 0;
  for (const TracePacket& trace_packet : trace_packets) {
    size_t num_slices_left_for_packet = trace_packet.slices().size();
    for (const Slice& slice : trace_packet.slices()) {
      // Check if this slice would cause the IPC to overflow its max size and,
      // if that is the case, split the IPCs. The "16" and "64" below are
      // over-estimations of, respectively:
      // 16: the preamble that prefixes each slice (there are 2 x size fields
      //     in the proto + the |last_slice_for_packet| bool).
      // 64: the overhead of the IPC InvokeMethodReply + wire_protocol's frame.
      // If these estimations are wrong, BufferedFrameDeserializer::Serialize()
      // will hit a DCHECK anyways.
      const size_t approx_slice_size = slice.size + 16;
      if (approx_reply_size + approx_slice_size > ipc::kIPCBufferSize - 64) {
        // If we hit this CHECK we got a single slice that is > kIPCBufferSize.
        PERFETTO_CHECK(result->slices_size() > 0);
        send_ipc_reply(/*has_more=*/true);
        approx_reply_size = 0;
      }
      approx_reply_size += approx_slice_size;

      auto* res_slice = result->add_slices();
      res_slice->set_last_slice_for_packet(--num_slices_left_for_packet == 0);
      res_slice->set_data(slice.start, slice.size);
    }
  }
  send_ipc_reply(has_more);
}

void ConsumerIPCService::RemoteConsumer::OnDetach(bool success) {
  if (!success) {
    std::move(detach_response).Reject();
    return;
  }
  auto resp = ipc::AsyncResult<protos::gen::DetachResponse>::Create();
  std::move(detach_response).Resolve(std::move(resp));
}

void ConsumerIPCService::RemoteConsumer::OnAttach(
    bool success,
    const TraceConfig& trace_config) {
  if (!success) {
    std::move(attach_response).Reject();
    return;
  }
  auto response = ipc::AsyncResult<protos::gen::AttachResponse>::Create();
  *response->mutable_trace_config() = trace_config;
  std::move(attach_response).Resolve(std::move(response));
}

void ConsumerIPCService::RemoteConsumer::OnTraceStats(bool success,
                                                      const TraceStats& stats) {
  if (!success) {
    std::move(get_trace_stats_response).Reject();
    return;
  }
  auto response =
      ipc::AsyncResult<protos::gen::GetTraceStatsResponse>::Create();
  *response->mutable_trace_stats() = stats;
  std::move(get_trace_stats_response).Resolve(std::move(response));
}

void ConsumerIPCService::RemoteConsumer::OnObservableEvents(
    const ObservableEvents& events) {
  if (!observe_events_response.IsBound())
    return;

  auto result = ipc::AsyncResult<protos::gen::ObserveEventsResponse>::Create();
  result.set_has_more(true);
  *result->mutable_events() = events;
  observe_events_response.Resolve(std::move(result));
}

void ConsumerIPCService::RemoteConsumer::CloseObserveEventsResponseStream() {
  if (!observe_events_response.IsBound())
    return;

  auto result = ipc::AsyncResult<protos::gen::ObserveEventsResponse>::Create();
  result.set_has_more(false);
  observe_events_response.Resolve(std::move(result));
}

}  // namespace perfetto
