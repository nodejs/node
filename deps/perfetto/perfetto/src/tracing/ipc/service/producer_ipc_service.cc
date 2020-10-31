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

#include "src/tracing/ipc/service/producer_ipc_service.h"

#include <inttypes.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/ipc/host.h"
#include "perfetto/ext/ipc/service.h"
#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "src/tracing/ipc/posix_shared_memory.h"

// The remote Producer(s) are not trusted. All the methods from the ProducerPort
// IPC layer (e.g. RegisterDataSource()) must assume that the remote Producer is
// compromised.

namespace perfetto {

ProducerIPCService::ProducerIPCService(TracingService* core_service)
    : core_service_(core_service), weak_ptr_factory_(this) {}

ProducerIPCService::~ProducerIPCService() = default;

ProducerIPCService::RemoteProducer*
ProducerIPCService::GetProducerForCurrentRequest() {
  const ipc::ClientID ipc_client_id = ipc::Service::client_info().client_id();
  PERFETTO_CHECK(ipc_client_id);
  auto it = producers_.find(ipc_client_id);
  if (it == producers_.end())
    return nullptr;
  return it->second.get();
}

// Called by the remote Producer through the IPC channel soon after connecting.
void ProducerIPCService::InitializeConnection(
    const protos::gen::InitializeConnectionRequest& req,
    DeferredInitializeConnectionResponse response) {
  const auto& client_info = ipc::Service::client_info();
  const ipc::ClientID ipc_client_id = client_info.client_id();
  PERFETTO_CHECK(ipc_client_id);

  if (producers_.count(ipc_client_id) > 0) {
    PERFETTO_DLOG(
        "The remote Producer is trying to re-initialize the connection");
    return response.Reject();
  }

  // Create a new entry.
  std::unique_ptr<RemoteProducer> producer(new RemoteProducer());

  TracingService::ProducerSMBScrapingMode smb_scraping_mode =
      TracingService::ProducerSMBScrapingMode::kDefault;
  switch (req.smb_scraping_mode()) {
    case protos::gen::InitializeConnectionRequest::SMB_SCRAPING_UNSPECIFIED:
      break;
    case protos::gen::InitializeConnectionRequest::SMB_SCRAPING_DISABLED:
      smb_scraping_mode = TracingService::ProducerSMBScrapingMode::kDisabled;
      break;
    case protos::gen::InitializeConnectionRequest::SMB_SCRAPING_ENABLED:
      smb_scraping_mode = TracingService::ProducerSMBScrapingMode::kEnabled;
      break;
  }

  bool dcheck_mismatch = false;
#if PERFETTO_DCHECK_IS_ON()
  dcheck_mismatch =
      req.build_flags() ==
      protos::gen::InitializeConnectionRequest::BUILD_FLAGS_DCHECKS_OFF;
#else
  dcheck_mismatch =
      req.build_flags() ==
      protos::gen::InitializeConnectionRequest::BUILD_FLAGS_DCHECKS_ON;
#endif
  if (dcheck_mismatch) {
    PERFETTO_LOG(
        "The producer and the service binaries are built using different "
        "DEBUG/NDEBUG flags. This will likely cause crashes.");
  }

  // If the producer provided an SMB, tell the service to attempt to adopt it.
  std::unique_ptr<SharedMemory> shmem;
  if (req.producer_provided_shmem()) {
    base::ScopedFile shmem_fd = ipc::Service::TakeReceivedFD();
    if (shmem_fd) {
      shmem = PosixSharedMemory::AttachToFd(
          std::move(shmem_fd), /*require_seals_if_supported=*/true);
      if (!shmem) {
        PERFETTO_ELOG(
            "Couldn't map producer-provided SMB, falling back to "
            "service-provided SMB");
      }
    } else {
      PERFETTO_DLOG(
          "InitializeConnectionRequest's producer_provided_shmem flag is set "
          "but the producer didn't provide an FD");
    }
  }

  // ConnectProducer will call OnConnect() on the next task.
  producer->service_endpoint = core_service_->ConnectProducer(
      producer.get(), client_info.uid(), req.producer_name(),
      req.shared_memory_size_hint_bytes(),
      /*in_process=*/false, smb_scraping_mode,
      req.shared_memory_page_size_hint_bytes(), std::move(shmem));

  // Could happen if the service has too many producers connected.
  if (!producer->service_endpoint) {
    response.Reject();
    return;
  }

  bool using_producer_shmem =
      producer->service_endpoint->IsShmemProvidedByProducer();

  producers_.emplace(ipc_client_id, std::move(producer));
  // Because of the std::move() |producer| is invalid after this point.

  auto async_res =
      ipc::AsyncResult<protos::gen::InitializeConnectionResponse>::Create();
  async_res->set_using_shmem_provided_by_producer(using_producer_shmem);
  response.Resolve(std::move(async_res));
}

// Called by the remote Producer through the IPC channel.
void ProducerIPCService::RegisterDataSource(
    const protos::gen::RegisterDataSourceRequest& req,
    DeferredRegisterDataSourceResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked RegisterDataSource() before InitializeConnection()");
    if (response.IsBound())
      response.Reject();
    return;
  }

  const DataSourceDescriptor& dsd = req.data_source_descriptor();
  GetProducerForCurrentRequest()->service_endpoint->RegisterDataSource(dsd);

  // RegisterDataSource doesn't expect any meaningful response.
  if (response.IsBound()) {
    response.Resolve(
        ipc::AsyncResult<protos::gen::RegisterDataSourceResponse>::Create());
  }
}

// Called by the IPC layer.
void ProducerIPCService::OnClientDisconnected() {
  ipc::ClientID client_id = ipc::Service::client_info().client_id();
  PERFETTO_DLOG("Client %" PRIu64 " disconnected", client_id);
  producers_.erase(client_id);
}

// TODO(fmayer): test what happens if we receive the following tasks, in order:
// RegisterDataSource, UnregisterDataSource, OnDataSourceRegistered.
// which essentially means that the client posted back to back a
// ReqisterDataSource and UnregisterDataSource speculating on the next id.
// Called by the remote Service through the IPC channel.
void ProducerIPCService::UnregisterDataSource(
    const protos::gen::UnregisterDataSourceRequest& req,
    DeferredUnregisterDataSourceResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked UnregisterDataSource() before "
        "InitializeConnection()");
    if (response.IsBound())
      response.Reject();
    return;
  }
  producer->service_endpoint->UnregisterDataSource(req.data_source_name());

  // UnregisterDataSource doesn't expect any meaningful response.
  if (response.IsBound()) {
    response.Resolve(
        ipc::AsyncResult<protos::gen::UnregisterDataSourceResponse>::Create());
  }
}

void ProducerIPCService::RegisterTraceWriter(
    const protos::gen::RegisterTraceWriterRequest& req,
    DeferredRegisterTraceWriterResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked RegisterTraceWriter() before "
        "InitializeConnection()");
    if (response.IsBound())
      response.Reject();
    return;
  }
  producer->service_endpoint->RegisterTraceWriter(req.trace_writer_id(),
                                                  req.target_buffer());

  // RegisterTraceWriter doesn't expect any meaningful response.
  if (response.IsBound()) {
    response.Resolve(
        ipc::AsyncResult<protos::gen::RegisterTraceWriterResponse>::Create());
  }
}

void ProducerIPCService::UnregisterTraceWriter(
    const protos::gen::UnregisterTraceWriterRequest& req,
    DeferredUnregisterTraceWriterResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked UnregisterTraceWriter() before "
        "InitializeConnection()");
    if (response.IsBound())
      response.Reject();
    return;
  }
  producer->service_endpoint->UnregisterTraceWriter(req.trace_writer_id());

  // UnregisterTraceWriter doesn't expect any meaningful response.
  if (response.IsBound()) {
    response.Resolve(
        ipc::AsyncResult<protos::gen::UnregisterTraceWriterResponse>::Create());
  }
}

void ProducerIPCService::CommitData(const protos::gen::CommitDataRequest& req,
                                    DeferredCommitDataResponse resp) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked CommitData() before InitializeConnection()");
    if (resp.IsBound())
      resp.Reject();
    return;
  }

  // We don't want to send a response if the client didn't attach a callback to
  // the original request. Doing so would generate unnecessary wakeups and
  // context switches.
  std::function<void()> callback;
  if (resp.IsBound()) {
    // Capturing |resp| by reference here speculates on the fact that
    // CommitData() in tracing_service_impl.cc invokes the passed callback
    // inline, without posting it. If that assumption changes this code needs to
    // wrap the response in a shared_ptr (C+11 lambdas don't support move) and
    // use a weak ptr in the caller.
    callback = [&resp] {
      resp.Resolve(ipc::AsyncResult<protos::gen::CommitDataResponse>::Create());
    };
  }
  producer->service_endpoint->CommitData(req, callback);
}

void ProducerIPCService::NotifyDataSourceStarted(
    const protos::gen::NotifyDataSourceStartedRequest& request,
    DeferredNotifyDataSourceStartedResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked NotifyDataSourceStarted() before "
        "InitializeConnection()");
    if (response.IsBound())
      response.Reject();
    return;
  }
  producer->service_endpoint->NotifyDataSourceStarted(request.data_source_id());

  // NotifyDataSourceStopped shouldn't expect any meaningful response, avoid
  // a useless IPC in that case.
  if (response.IsBound()) {
    response.Resolve(ipc::AsyncResult<
                     protos::gen::NotifyDataSourceStartedResponse>::Create());
  }
}

void ProducerIPCService::NotifyDataSourceStopped(
    const protos::gen::NotifyDataSourceStoppedRequest& request,
    DeferredNotifyDataSourceStoppedResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked NotifyDataSourceStopped() before "
        "InitializeConnection()");
    if (response.IsBound())
      response.Reject();
    return;
  }
  producer->service_endpoint->NotifyDataSourceStopped(request.data_source_id());

  // NotifyDataSourceStopped shouldn't expect any meaningful response, avoid
  // a useless IPC in that case.
  if (response.IsBound()) {
    response.Resolve(ipc::AsyncResult<
                     protos::gen::NotifyDataSourceStoppedResponse>::Create());
  }
}

void ProducerIPCService::ActivateTriggers(
    const protos::gen::ActivateTriggersRequest& proto_req,
    DeferredActivateTriggersResponse resp) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked ActivateTriggers() before InitializeConnection()");
    if (resp.IsBound())
      resp.Reject();
    return;
  }
  std::vector<std::string> triggers;
  for (const auto& name : proto_req.trigger_names()) {
    triggers.push_back(name);
  }
  producer->service_endpoint->ActivateTriggers(triggers);
  // ActivateTriggers shouldn't expect any meaningful response, avoid
  // a useless IPC in that case.
  if (resp.IsBound()) {
    resp.Resolve(
        ipc::AsyncResult<protos::gen::ActivateTriggersResponse>::Create());
  }
}

void ProducerIPCService::GetAsyncCommand(
    const protos::gen::GetAsyncCommandRequest&,
    DeferredGetAsyncCommandResponse response) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG(
        "Producer invoked GetAsyncCommand() before "
        "InitializeConnection()");
    return response.Reject();
  }
  // Keep the back channel open, without ever resolving the ipc::Deferred fully,
  // to send async commands to the RemoteProducer (e.g., starting/stopping a
  // data source).
  producer->async_producer_commands = std::move(response);

  // Service may already have issued the OnTracingSetup() event, in which case
  // we should forward it to the producer now.
  if (producer->send_setup_tracing_on_async_commands_bound)
    producer->SendSetupTracing();
}

void ProducerIPCService::Sync(const protos::gen::SyncRequest&,
                              DeferredSyncResponse resp) {
  RemoteProducer* producer = GetProducerForCurrentRequest();
  if (!producer) {
    PERFETTO_DLOG("Producer invoked Sync() before InitializeConnection()");
    return resp.Reject();
  }
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto resp_it = pending_syncs_.insert(pending_syncs_.end(), std::move(resp));
  auto callback = [weak_this, resp_it]() {
    if (!weak_this)
      return;
    auto pending_resp = std::move(*resp_it);
    weak_this->pending_syncs_.erase(resp_it);
    pending_resp.Resolve(ipc::AsyncResult<protos::gen::SyncResponse>::Create());
  };
  producer->service_endpoint->Sync(callback);
}

////////////////////////////////////////////////////////////////////////////////
// RemoteProducer methods
////////////////////////////////////////////////////////////////////////////////

ProducerIPCService::RemoteProducer::RemoteProducer() = default;
ProducerIPCService::RemoteProducer::~RemoteProducer() = default;

// Invoked by the |core_service_| business logic after the ConnectProducer()
// call. There is nothing to do here, we really expected the ConnectProducer()
// to just work in the local case.
void ProducerIPCService::RemoteProducer::OnConnect() {}

// Invoked by the |core_service_| business logic after we destroy the
// |service_endpoint| (in the RemoteProducer dtor).
void ProducerIPCService::RemoteProducer::OnDisconnect() {}

// Invoked by the |core_service_| business logic when it wants to create a new
// data source.
void ProducerIPCService::RemoteProducer::SetupDataSource(
    DataSourceInstanceID dsid,
    const DataSourceConfig& cfg) {
  if (!async_producer_commands.IsBound()) {
    PERFETTO_DLOG(
        "The Service tried to create a new data source but the remote Producer "
        "has not yet initialized the connection");
    return;
  }
  auto cmd = ipc::AsyncResult<protos::gen::GetAsyncCommandResponse>::Create();
  cmd.set_has_more(true);
  cmd->mutable_setup_data_source()->set_new_instance_id(dsid);
  *cmd->mutable_setup_data_source()->mutable_config() = cfg;
  async_producer_commands.Resolve(std::move(cmd));
}

// Invoked by the |core_service_| business logic when it wants to start a new
// data source.
void ProducerIPCService::RemoteProducer::StartDataSource(
    DataSourceInstanceID dsid,
    const DataSourceConfig& cfg) {
  if (!async_producer_commands.IsBound()) {
    PERFETTO_DLOG(
        "The Service tried to start a new data source but the remote Producer "
        "has not yet initialized the connection");
    return;
  }
  auto cmd = ipc::AsyncResult<protos::gen::GetAsyncCommandResponse>::Create();
  cmd.set_has_more(true);
  cmd->mutable_start_data_source()->set_new_instance_id(dsid);
  *cmd->mutable_start_data_source()->mutable_config() = cfg;
  async_producer_commands.Resolve(std::move(cmd));
}

void ProducerIPCService::RemoteProducer::StopDataSource(
    DataSourceInstanceID dsid) {
  if (!async_producer_commands.IsBound()) {
    PERFETTO_DLOG(
        "The Service tried to stop a data source but the remote Producer "
        "has not yet initialized the connection");
    return;
  }
  auto cmd = ipc::AsyncResult<protos::gen::GetAsyncCommandResponse>::Create();
  cmd.set_has_more(true);
  cmd->mutable_stop_data_source()->set_instance_id(dsid);
  async_producer_commands.Resolve(std::move(cmd));
}

void ProducerIPCService::RemoteProducer::OnTracingSetup() {
  if (!async_producer_commands.IsBound()) {
    // Service may call this before the producer issued GetAsyncCommand.
    send_setup_tracing_on_async_commands_bound = true;
    return;
  }
  SendSetupTracing();
}

void ProducerIPCService::RemoteProducer::SendSetupTracing() {
  PERFETTO_CHECK(async_producer_commands.IsBound());
  PERFETTO_CHECK(service_endpoint->shared_memory());
  auto cmd = ipc::AsyncResult<protos::gen::GetAsyncCommandResponse>::Create();
  cmd.set_has_more(true);
  auto setup_tracing = cmd->mutable_setup_tracing();
  if (!service_endpoint->IsShmemProvidedByProducer()) {
    // Nominal case (% Chrome): service provides SMB.
    setup_tracing->set_shared_buffer_page_size_kb(
        static_cast<uint32_t>(service_endpoint->shared_buffer_page_size_kb()));
    const int shm_fd =
        static_cast<PosixSharedMemory*>(service_endpoint->shared_memory())
            ->fd();
    cmd.set_fd(shm_fd);
  }
  async_producer_commands.Resolve(std::move(cmd));
}

void ProducerIPCService::RemoteProducer::Flush(
    FlushRequestID flush_request_id,
    const DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {
  if (!async_producer_commands.IsBound()) {
    PERFETTO_DLOG(
        "The Service tried to request a flush but the remote Producer has not "
        "yet initialized the connection");
    return;
  }
  auto cmd = ipc::AsyncResult<protos::gen::GetAsyncCommandResponse>::Create();
  cmd.set_has_more(true);
  for (size_t i = 0; i < num_data_sources; i++)
    cmd->mutable_flush()->add_data_source_ids(data_source_ids[i]);
  cmd->mutable_flush()->set_request_id(flush_request_id);
  async_producer_commands.Resolve(std::move(cmd));
}

void ProducerIPCService::RemoteProducer::ClearIncrementalState(
    const DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {
  if (!async_producer_commands.IsBound()) {
    PERFETTO_DLOG(
        "The Service tried to request an incremental state invalidation, but "
        "the remote Producer has not yet initialized the connection");
    return;
  }
  auto cmd = ipc::AsyncResult<protos::gen::GetAsyncCommandResponse>::Create();
  cmd.set_has_more(true);
  for (size_t i = 0; i < num_data_sources; i++)
    cmd->mutable_clear_incremental_state()->add_data_source_ids(
        data_source_ids[i]);
  async_producer_commands.Resolve(std::move(cmd));
}

}  // namespace perfetto
