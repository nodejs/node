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

#ifndef SRC_TRACING_IPC_SERVICE_PRODUCER_IPC_SERVICE_H_
#define SRC_TRACING_IPC_SERVICE_PRODUCER_IPC_SERVICE_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/ipc/basic_types.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"

#include "protos/perfetto/ipc/producer_port.ipc.h"

namespace perfetto {

namespace ipc {
class Host;
}  // namespace ipc

// Implements the Producer port of the IPC service. This class proxies requests
// and responses between the core service logic (|svc_|) and remote Producer(s)
// on the IPC socket, through the methods overriddden from ProducerPort.
class ProducerIPCService : public protos::gen::ProducerPort {
 public:
  explicit ProducerIPCService(TracingService* core_service);
  ~ProducerIPCService() override;

  // ProducerPort implementation (from .proto IPC definition).
  void InitializeConnection(const protos::gen::InitializeConnectionRequest&,
                            DeferredInitializeConnectionResponse) override;
  void RegisterDataSource(const protos::gen::RegisterDataSourceRequest&,
                          DeferredRegisterDataSourceResponse) override;
  void UnregisterDataSource(const protos::gen::UnregisterDataSourceRequest&,
                            DeferredUnregisterDataSourceResponse) override;
  void RegisterTraceWriter(const protos::gen::RegisterTraceWriterRequest&,
                           DeferredRegisterTraceWriterResponse) override;
  void UnregisterTraceWriter(const protos::gen::UnregisterTraceWriterRequest&,
                             DeferredUnregisterTraceWriterResponse) override;
  void CommitData(const protos::gen::CommitDataRequest&,
                  DeferredCommitDataResponse) override;
  void NotifyDataSourceStarted(
      const protos::gen::NotifyDataSourceStartedRequest&,
      DeferredNotifyDataSourceStartedResponse) override;
  void NotifyDataSourceStopped(
      const protos::gen::NotifyDataSourceStoppedRequest&,
      DeferredNotifyDataSourceStoppedResponse) override;
  void ActivateTriggers(const protos::gen::ActivateTriggersRequest&,
                        DeferredActivateTriggersResponse) override;

  void GetAsyncCommand(const protos::gen::GetAsyncCommandRequest&,
                       DeferredGetAsyncCommandResponse) override;
  void Sync(const protos::gen::SyncRequest&, DeferredSyncResponse) override;
  void OnClientDisconnected() override;

 private:
  // Acts like a Producer with the core Service business logic (which doesn't
  // know anything about the remote transport), but all it does is proxying
  // methods to the remote Producer on the other side of the IPC channel.
  class RemoteProducer : public Producer {
   public:
    RemoteProducer();
    ~RemoteProducer() override;

    // These methods are called by the |core_service_| business logic. There is
    // no connection here, these methods are posted straight away.
    void OnConnect() override;
    void OnDisconnect() override;
    void SetupDataSource(DataSourceInstanceID,
                         const DataSourceConfig&) override;
    void StartDataSource(DataSourceInstanceID,
                         const DataSourceConfig&) override;
    void StopDataSource(DataSourceInstanceID) override;
    void OnTracingSetup() override;
    void Flush(FlushRequestID,
               const DataSourceInstanceID* data_source_ids,
               size_t num_data_sources) override;

    void ClearIncrementalState(const DataSourceInstanceID* data_source_ids,
                               size_t num_data_sources) override;

    void SendSetupTracing();

    // The interface obtained from the core service business logic through
    // Service::ConnectProducer(this). This allows to invoke methods for a
    // specific Producer on the Service business logic.
    std::unique_ptr<TracingService::ProducerEndpoint> service_endpoint;

    // The back-channel (based on a never ending stream request) that allows us
    // to send asynchronous commands to the remote Producer (e.g. start/stop a
    // data source).
    DeferredGetAsyncCommandResponse async_producer_commands;

    // Set if the service calls OnTracingSetup() before the
    // |async_producer_commands| was bound by the service. In this case, we
    // forward the SetupTracing command when it is bound later.
    bool send_setup_tracing_on_async_commands_bound = false;
  };

  ProducerIPCService(const ProducerIPCService&) = delete;
  ProducerIPCService& operator=(const ProducerIPCService&) = delete;

  // Returns the ProducerEndpoint in the core business logic that corresponds to
  // the current IPC request.
  RemoteProducer* GetProducerForCurrentRequest();

  TracingService* const core_service_;

  // Maps IPC clients to ProducerEndpoint instances registered on the
  // |core_service_| business logic.
  std::map<ipc::ClientID, std::unique_ptr<RemoteProducer>> producers_;

  // List because pointers need to be stable.
  std::list<DeferredSyncResponse> pending_syncs_;

  base::WeakPtrFactory<ProducerIPCService> weak_ptr_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACING_IPC_SERVICE_PRODUCER_IPC_SERVICE_H_
