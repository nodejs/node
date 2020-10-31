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

#include "src/tracing/ipc/consumer/consumer_ipc_client_impl.h"

#include <inttypes.h>
#include <string.h>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/ipc/client.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/observable_events.h"
#include "perfetto/ext/tracing/core/trace_stats.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/core/tracing_service_state.h"

// TODO(fmayer): Add a test to check to what happens when ConsumerIPCClientImpl
// gets destroyed w.r.t. the Consumer pointer. Also think to lifetime of the
// Consumer* during the callbacks.

namespace perfetto {

// static. (Declared in include/tracing/ipc/consumer_ipc_client.h).
std::unique_ptr<TracingService::ConsumerEndpoint> ConsumerIPCClient::Connect(
    const char* service_sock_name,
    Consumer* consumer,
    base::TaskRunner* task_runner) {
  return std::unique_ptr<TracingService::ConsumerEndpoint>(
      new ConsumerIPCClientImpl(service_sock_name, consumer, task_runner));
}

ConsumerIPCClientImpl::ConsumerIPCClientImpl(const char* service_sock_name,
                                             Consumer* consumer,
                                             base::TaskRunner* task_runner)
    : consumer_(consumer),
      ipc_channel_(ipc::Client::CreateInstance(service_sock_name, task_runner)),
      consumer_port_(this /* event_listener */),
      weak_ptr_factory_(this) {
  ipc_channel_->BindService(consumer_port_.GetWeakPtr());
}

ConsumerIPCClientImpl::~ConsumerIPCClientImpl() = default;

// Called by the IPC layer if the BindService() succeeds.
void ConsumerIPCClientImpl::OnConnect() {
  connected_ = true;
  consumer_->OnConnect();
}

void ConsumerIPCClientImpl::OnDisconnect() {
  PERFETTO_DLOG("Tracing service connection failure");
  connected_ = false;
  consumer_->OnDisconnect();
}

void ConsumerIPCClientImpl::EnableTracing(const TraceConfig& trace_config,
                                          base::ScopedFile fd) {
  if (!connected_) {
    PERFETTO_DLOG("Cannot EnableTracing(), not connected to tracing service");
    return;
  }

  protos::gen::EnableTracingRequest req;
  *req.mutable_trace_config() = trace_config;
  ipc::Deferred<protos::gen::EnableTracingResponse> async_response;
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  async_response.Bind(
      [weak_this](
          ipc::AsyncResult<protos::gen::EnableTracingResponse> response) {
        if (weak_this)
          weak_this->OnEnableTracingResponse(std::move(response));
      });

  // |fd| will be closed when this function returns, but it's fine because the
  // IPC layer dup()'s it when sending the IPC.
  consumer_port_.EnableTracing(req, std::move(async_response), *fd);
}

void ConsumerIPCClientImpl::ChangeTraceConfig(const TraceConfig&) {
  if (!connected_) {
    PERFETTO_DLOG(
        "Cannot ChangeTraceConfig(), not connected to tracing service");
    return;
  }

  ipc::Deferred<protos::gen::ChangeTraceConfigResponse> async_response;
  async_response.Bind(
      [](ipc::AsyncResult<protos::gen::ChangeTraceConfigResponse> response) {
        if (!response)
          PERFETTO_DLOG("ChangeTraceConfig() failed");
      });
  protos::gen::ChangeTraceConfigRequest req;
  consumer_port_.ChangeTraceConfig(req, std::move(async_response));
}

void ConsumerIPCClientImpl::StartTracing() {
  if (!connected_) {
    PERFETTO_DLOG("Cannot StartTracing(), not connected to tracing service");
    return;
  }

  ipc::Deferred<protos::gen::StartTracingResponse> async_response;
  async_response.Bind(
      [](ipc::AsyncResult<protos::gen::StartTracingResponse> response) {
        if (!response)
          PERFETTO_DLOG("StartTracing() failed");
      });
  protos::gen::StartTracingRequest req;
  consumer_port_.StartTracing(req, std::move(async_response));
}

void ConsumerIPCClientImpl::DisableTracing() {
  if (!connected_) {
    PERFETTO_DLOG("Cannot DisableTracing(), not connected to tracing service");
    return;
  }

  ipc::Deferred<protos::gen::DisableTracingResponse> async_response;
  async_response.Bind(
      [](ipc::AsyncResult<protos::gen::DisableTracingResponse> response) {
        if (!response)
          PERFETTO_DLOG("DisableTracing() failed");
      });
  consumer_port_.DisableTracing(protos::gen::DisableTracingRequest(),
                                std::move(async_response));
}

void ConsumerIPCClientImpl::ReadBuffers() {
  if (!connected_) {
    PERFETTO_DLOG("Cannot ReadBuffers(), not connected to tracing service");
    return;
  }

  ipc::Deferred<protos::gen::ReadBuffersResponse> async_response;

  // The IPC layer guarantees that callbacks are destroyed after this object
  // is destroyed (by virtue of destroying the |consumer_port_|). In turn the
  // contract of this class expects the caller to not destroy the Consumer class
  // before having destroyed this class. Hence binding |this| here is safe.
  async_response.Bind(
      [this](ipc::AsyncResult<protos::gen::ReadBuffersResponse> response) {
        OnReadBuffersResponse(std::move(response));
      });
  consumer_port_.ReadBuffers(protos::gen::ReadBuffersRequest(),
                             std::move(async_response));
}

void ConsumerIPCClientImpl::OnReadBuffersResponse(
    ipc::AsyncResult<protos::gen::ReadBuffersResponse> response) {
  if (!response) {
    PERFETTO_DLOG("ReadBuffers() failed");
    return;
  }
  std::vector<TracePacket> trace_packets;
  for (auto& resp_slice : response->slices()) {
    const std::string& slice_data = resp_slice.data();
    Slice slice = Slice::Allocate(slice_data.size());
    memcpy(slice.own_data(), slice_data.data(), slice.size);
    partial_packet_.AddSlice(std::move(slice));
    if (resp_slice.last_slice_for_packet())
      trace_packets.emplace_back(std::move(partial_packet_));
  }
  if (!trace_packets.empty() || !response.has_more())
    consumer_->OnTraceData(std::move(trace_packets), response.has_more());
}

void ConsumerIPCClientImpl::OnEnableTracingResponse(
    ipc::AsyncResult<protos::gen::EnableTracingResponse> response) {
  if (!response || response->disabled())
    consumer_->OnTracingDisabled();
}

void ConsumerIPCClientImpl::FreeBuffers() {
  if (!connected_) {
    PERFETTO_DLOG("Cannot FreeBuffers(), not connected to tracing service");
    return;
  }

  protos::gen::FreeBuffersRequest req;
  ipc::Deferred<protos::gen::FreeBuffersResponse> async_response;
  async_response.Bind(
      [](ipc::AsyncResult<protos::gen::FreeBuffersResponse> response) {
        if (!response)
          PERFETTO_DLOG("FreeBuffers() failed");
      });
  consumer_port_.FreeBuffers(req, std::move(async_response));
}

void ConsumerIPCClientImpl::Flush(uint32_t timeout_ms, FlushCallback callback) {
  if (!connected_) {
    PERFETTO_DLOG("Cannot Flush(), not connected to tracing service");
    return callback(/*success=*/false);
  }

  protos::gen::FlushRequest req;
  req.set_timeout_ms(static_cast<uint32_t>(timeout_ms));
  ipc::Deferred<protos::gen::FlushResponse> async_response;
  async_response.Bind(
      [callback](ipc::AsyncResult<protos::gen::FlushResponse> response) {
        callback(!!response);
      });
  consumer_port_.Flush(req, std::move(async_response));
}

void ConsumerIPCClientImpl::Detach(const std::string& key) {
  if (!connected_) {
    PERFETTO_DLOG("Cannot Detach(), not connected to tracing service");
    return;
  }

  protos::gen::DetachRequest req;
  req.set_key(key);
  ipc::Deferred<protos::gen::DetachResponse> async_response;
  auto weak_this = weak_ptr_factory_.GetWeakPtr();

  async_response.Bind(
      [weak_this](ipc::AsyncResult<protos::gen::DetachResponse> response) {
        if (weak_this)
          weak_this->consumer_->OnDetach(!!response);
      });
  consumer_port_.Detach(req, std::move(async_response));
}

void ConsumerIPCClientImpl::Attach(const std::string& key) {
  if (!connected_) {
    PERFETTO_DLOG("Cannot Attach(), not connected to tracing service");
    return;
  }

  {
    protos::gen::AttachRequest req;
    req.set_key(key);
    ipc::Deferred<protos::gen::AttachResponse> async_response;
    auto weak_this = weak_ptr_factory_.GetWeakPtr();

    async_response.Bind(
        [weak_this](ipc::AsyncResult<protos::gen::AttachResponse> response) {
          if (!weak_this)
            return;
          if (!response) {
            weak_this->consumer_->OnAttach(/*success=*/false, TraceConfig());
            return;
          }
          const TraceConfig& trace_config = response->trace_config();

          // If attached succesfully, also attach to the end-of-trace
          // notificaton callback, via EnableTracing(attach_notification_only).
          protos::gen::EnableTracingRequest enable_req;
          enable_req.set_attach_notification_only(true);
          ipc::Deferred<protos::gen::EnableTracingResponse> enable_resp;
          enable_resp.Bind(
              [weak_this](
                  ipc::AsyncResult<protos::gen::EnableTracingResponse> resp) {
                if (weak_this)
                  weak_this->OnEnableTracingResponse(std::move(resp));
              });
          weak_this->consumer_port_.EnableTracing(enable_req,
                                                  std::move(enable_resp));

          weak_this->consumer_->OnAttach(/*success=*/true, trace_config);
        });
    consumer_port_.Attach(req, std::move(async_response));
  }
}

void ConsumerIPCClientImpl::GetTraceStats() {
  if (!connected_) {
    PERFETTO_DLOG("Cannot GetTraceStats(), not connected to tracing service");
    return;
  }

  protos::gen::GetTraceStatsRequest req;
  ipc::Deferred<protos::gen::GetTraceStatsResponse> async_response;

  // The IPC layer guarantees that callbacks are destroyed after this object
  // is destroyed (by virtue of destroying the |consumer_port_|). In turn the
  // contract of this class expects the caller to not destroy the Consumer class
  // before having destroyed this class. Hence binding |this| here is safe.
  async_response.Bind(
      [this](ipc::AsyncResult<protos::gen::GetTraceStatsResponse> response) {
        if (!response) {
          consumer_->OnTraceStats(/*success=*/false, TraceStats());
          return;
        }
        consumer_->OnTraceStats(/*success=*/true, response->trace_stats());
      });
  consumer_port_.GetTraceStats(req, std::move(async_response));
}

void ConsumerIPCClientImpl::ObserveEvents(uint32_t enabled_event_types) {
  if (!connected_) {
    PERFETTO_DLOG("Cannot ObserveEvents(), not connected to tracing service");
    return;
  }

  protos::gen::ObserveEventsRequest req;
  for (uint32_t i = 0; i < 32; i++) {
    const uint32_t event_id = 1u << i;
    if (enabled_event_types & event_id)
      req.add_events_to_observe(static_cast<ObservableEvents::Type>(event_id));
  }

  ipc::Deferred<protos::gen::ObserveEventsResponse> async_response;
  // The IPC layer guarantees that callbacks are destroyed after this object
  // is destroyed (by virtue of destroying the |consumer_port_|). In turn the
  // contract of this class expects the caller to not destroy the Consumer class
  // before having destroyed this class. Hence binding |this| here is safe.
  async_response.Bind(
      [this](ipc::AsyncResult<protos::gen::ObserveEventsResponse> response) {
        // Skip empty response, which the service sends to close the stream.
        if (!response.has_more()) {
          PERFETTO_DCHECK(!response->events().instance_state_changes().size());
          return;
        }
        consumer_->OnObservableEvents(response->events());
      });
  consumer_port_.ObserveEvents(req, std::move(async_response));
}

void ConsumerIPCClientImpl::QueryServiceState(
    QueryServiceStateCallback callback) {
  if (!connected_) {
    PERFETTO_DLOG(
        "Cannot QueryServiceState(), not connected to tracing service");
    return;
  }

  auto it = pending_query_svc_reqs_.insert(pending_query_svc_reqs_.end(),
                                           {std::move(callback), {}});
  protos::gen::QueryServiceStateRequest req;
  ipc::Deferred<protos::gen::QueryServiceStateResponse> async_response;
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  async_response.Bind(
      [weak_this,
       it](ipc::AsyncResult<protos::gen::QueryServiceStateResponse> response) {
        if (weak_this)
          weak_this->OnQueryServiceStateResponse(std::move(response), it);
      });
  consumer_port_.QueryServiceState(req, std::move(async_response));
}

void ConsumerIPCClientImpl::OnQueryServiceStateResponse(
    ipc::AsyncResult<protos::gen::QueryServiceStateResponse> response,
    PendingQueryServiceRequests::iterator req_it) {
  PERFETTO_DCHECK(req_it->callback);

  if (!response) {
    auto callback = std::move(req_it->callback);
    pending_query_svc_reqs_.erase(req_it);
    callback(false, TracingServiceState());
    return;
  }

  // The QueryServiceState response can be split in several chunks if the
  // service has several data sources. The client is supposed to merge all the
  // replies. The easiest way to achieve this is to re-serialize the partial
  // response and then re-decode the merged result in one shot.
  std::vector<uint8_t>& merged_resp = req_it->merged_resp;
  std::vector<uint8_t> part = response->service_state().SerializeAsArray();
  merged_resp.insert(merged_resp.end(), part.begin(), part.end());

  if (response.has_more())
    return;

  // All replies have been received. Decode the merged result and reply to the
  // callback.
  protos::gen::TracingServiceState svc_state;
  bool ok = svc_state.ParseFromArray(merged_resp.data(), merged_resp.size());
  if (!ok)
    PERFETTO_ELOG("Failed to decode merged QueryServiceStateResponse");
  auto callback = std::move(req_it->callback);
  pending_query_svc_reqs_.erase(req_it);
  callback(ok, std::move(svc_state));
}

void ConsumerIPCClientImpl::QueryCapabilities(
    QueryCapabilitiesCallback callback) {
  if (!connected_) {
    PERFETTO_DLOG(
        "Cannot QueryCapabilities(), not connected to tracing service");
    return;
  }

  protos::gen::QueryCapabilitiesRequest req;
  ipc::Deferred<protos::gen::QueryCapabilitiesResponse> async_response;
  async_response.Bind(
      [callback](
          ipc::AsyncResult<protos::gen::QueryCapabilitiesResponse> response) {
        if (!response) {
          // If the IPC fails, we are talking to an older version of the service
          // that didn't support QueryCapabilities at all. In this case return
          // an empty capabilities message.
          callback(TracingServiceCapabilities());
        } else {
          callback(response->capabilities());
        }
      });
  consumer_port_.QueryCapabilities(req, std::move(async_response));
}

}  // namespace perfetto
