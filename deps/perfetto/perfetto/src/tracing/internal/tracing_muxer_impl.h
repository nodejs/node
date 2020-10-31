/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACING_INTERNAL_TRACING_MUXER_IMPL_H_
#define SRC_TRACING_INTERNAL_TRACING_MUXER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <bitset>
#include <map>
#include <memory>
#include <vector>

#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/forward_decls.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/internal/basic_types.h"
#include "perfetto/tracing/internal/tracing_muxer.h"
#include "perfetto/tracing/tracing.h"
namespace perfetto {

class ConsumerEndpoint;
class DataSourceBase;
class ProducerEndpoint;
class TraceWriterBase;
class TracingBackend;
class TracingSession;
struct TracingInitArgs;

namespace base {
class TaskRunner;
}

namespace internal {

struct DataSourceStaticState;

// This class acts as a bridge between the public API and the TracingBackend(s).
// It exposes a simplified view of the world to the API methods handling all the
// bookkeeping to map data source instances and trace writers to the various
// backends. It deals with N data sources, M backends (1 backend == 1 tracing
// service == 1 producer connection) and T concurrent tracing sessions.
//
// Handing data source registration and start/stop flows [producer side]:
// ----------------------------------------------------------------------
// 1. The API client subclasses perfetto::DataSource and calls
//    DataSource::Register<MyDataSource>(). In turn this calls into the
//    TracingMuxer.
// 2. The tracing muxer iterates through all the backends (1 backend == 1
//    service == 1 producer connection) and registers the data source on each
//    backend.
// 3. When any (services behind a) backend starts tracing and requests to start
//    that specific data source, the TracingMuxerImpl constructs a new instance
//    of MyDataSource and calls the OnStart() method.
//
// Controlling trace and retrieving trace data [consumer side]:
// ------------------------------------------------------------
// 1. The API client calls Tracing::NewTrace(), returns a RAII TracingSession
//    object.
// 2. NewTrace() calls into internal::TracingMuxer(Impl). TracingMuxer
//    subclasses the TracingSession object (TracingSessionImpl) and returns it.
// 3. The tracing muxer identifies the backend (according to the args passed to
//    NewTrace), creates a new Consumer and connects to it.
// 4. When the API client calls Start()/Stop()/ReadTrace() methods, the
//    TracingMuxer forwards them to the consumer associated to the
//    TracingSession. Likewise for callbacks coming from the consumer-side of
//    the service.
class TracingMuxerImpl : public TracingMuxer {
 public:
  // This is different than TracingSessionID because it's global across all
  // backends. TracingSessionID is global only within the scope of one service.
  using TracingSessionGlobalID = uint64_t;

  static void InitializeInstance(const TracingInitArgs&);

  // TracingMuxer implementation.
  bool RegisterDataSource(const DataSourceDescriptor&,
                          DataSourceFactory,
                          DataSourceStaticState*) override;
  std::unique_ptr<TraceWriterBase> CreateTraceWriter(
      DataSourceState*,
      BufferExhaustedPolicy buffer_exhausted_policy) override;
  void DestroyStoppedTraceWritersForCurrentThread() override;

  std::unique_ptr<TracingSession> CreateTracingSession(BackendType);

  // Producer-side bookkeeping methods.
  void UpdateDataSourcesOnAllBackends();
  void SetupDataSource(TracingBackendId,
                       DataSourceInstanceID,
                       const DataSourceConfig&);
  void StartDataSource(TracingBackendId, DataSourceInstanceID);
  void StopDataSource_AsyncBegin(TracingBackendId, DataSourceInstanceID);
  void StopDataSource_AsyncEnd(TracingBackendId, DataSourceInstanceID);

  // Consumer-side bookkeeping methods.
  void SetupTracingSession(TracingSessionGlobalID,
                           const std::shared_ptr<TraceConfig>&,
                           base::ScopedFile trace_fd = base::ScopedFile());
  void StartTracingSession(TracingSessionGlobalID);
  void StopTracingSession(TracingSessionGlobalID);
  void DestroyTracingSession(TracingSessionGlobalID);
  void ReadTracingSessionData(
      TracingSessionGlobalID,
      std::function<void(TracingSession::ReadTraceCallbackArgs)>);

 private:
  // For each TracingBackend we create and register one ProducerImpl instance.
  // This talks to the producer-side of the service, gets start/stop requests
  // from it and routes them to the registered data sources.
  // One ProducerImpl == one backend == one tracing service.
  // This class is needed to disambiguate callbacks coming from different
  // services. TracingMuxerImpl can't directly implement the Producer interface
  // because the Producer virtual methods don't allow to identify the service.
  class ProducerImpl : public Producer {
   public:
    ProducerImpl(TracingMuxerImpl*, TracingBackendId);
    ~ProducerImpl() override;

    void Initialize(std::unique_ptr<ProducerEndpoint> endpoint);
    void RegisterDataSource(const DataSourceDescriptor&,
                            DataSourceFactory,
                            DataSourceStaticState*);

    // perfetto::Producer implementation.
    void OnConnect() override;
    void OnDisconnect() override;
    void OnTracingSetup() override;
    void SetupDataSource(DataSourceInstanceID,
                         const DataSourceConfig&) override;
    void StartDataSource(DataSourceInstanceID,
                         const DataSourceConfig&) override;
    void StopDataSource(DataSourceInstanceID) override;
    void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override;
    void ClearIncrementalState(const DataSourceInstanceID*, size_t) override;

    PERFETTO_THREAD_CHECKER(thread_checker_)
    TracingMuxerImpl* const muxer_;
    TracingBackendId const backend_id_;
    bool connected_ = false;

    // Set of data sources that have been actually registered on this producer.
    // This can be a subset of the global |data_sources_|, because data sources
    // can register before the producer is fully connected.
    std::bitset<kMaxDataSources> registered_data_sources_{};

    std::unique_ptr<ProducerEndpoint> service_;  // Keep last.
  };

  // For each TracingSession created by the API client (Tracing::NewTrace() we
  // create and register one ConsumerImpl instance.
  // This talks to the consumer-side of the service, gets end-of-trace and
  // on-trace-data callbacks and routes them to the API client callbacks.
  // This class is needed to disambiguate callbacks coming from different
  // tracing sessions.
  class ConsumerImpl : public Consumer {
   public:
    ConsumerImpl(TracingMuxerImpl*, TracingBackendId, TracingSessionGlobalID);
    ~ConsumerImpl() override;

    void Initialize(std::unique_ptr<ConsumerEndpoint> endpoint);

    // perfetto::Consumer implementation.
    void OnConnect() override;
    void OnDisconnect() override;
    void OnTracingDisabled() override;
    void OnTraceData(std::vector<TracePacket>, bool has_more) override;
    void OnDetach(bool success) override;
    void OnAttach(bool success, const TraceConfig&) override;
    void OnTraceStats(bool success, const TraceStats&) override;
    void OnObservableEvents(const ObservableEvents&) override;

    void NotifyStartComplete();
    void NotifyStopComplete();

    // Will eventually inform the |muxer_| when it is safe to remove |this|.
    void Disconnect();

    TracingMuxerImpl* const muxer_;
    TracingBackendId const backend_id_;
    TracingSessionGlobalID const session_id_;
    bool connected_ = false;

    // This is to handle the case where the Setup call from the API client
    // arrives before the consumer has connected. In this case we keep around
    // the config and check if we have it after connection.
    bool start_pending_ = false;

    // Similarly if the session is stopped before the consumer was connected, we
    // need to wait until the session has started before stopping it.
    bool stop_pending_ = false;

    // Whether this session was already stopped. This will happen in response to
    // Stop{,Blocking}, but also if the service stops the session for us
    // automatically (e.g., when there are no data sources).
    bool stopped_ = false;

    // shared_ptr because it's posted across threads. This is to avoid copying
    // it more than once.
    std::shared_ptr<TraceConfig> trace_config_;
    base::ScopedFile trace_fd_;

    // An internal callback used to implement StartBlocking().
    std::function<void()> blocking_start_complete_callback_;

    // If the API client passes a callback to stop, we should invoke this when
    // OnTracingDisabled() is invoked.
    std::function<void()> stop_complete_callback_;

    // An internal callback used to implement StopBlocking().
    std::function<void()> blocking_stop_complete_callback_;

    // Callback passed to ReadTrace().
    std::function<void(TracingSession::ReadTraceCallbackArgs)>
        read_trace_callback_;

    // The states of all data sources in this tracing session. |true| means the
    // data source has started tracing.
    using DataSourceHandle = std::pair<std::string, std::string>;
    std::map<DataSourceHandle, bool> data_source_states_;

    std::unique_ptr<ConsumerEndpoint> service_;  // Keep before last.
    PERFETTO_THREAD_CHECKER(thread_checker_)     // Keep last.
  };

  // This object is returned to API clients when they call
  // Tracing::CreateTracingSession().
  class TracingSessionImpl : public TracingSession {
   public:
    TracingSessionImpl(TracingMuxerImpl*, TracingSessionGlobalID);
    ~TracingSessionImpl() override;
    void Setup(const TraceConfig&, int fd) override;
    void Start() override;
    void StartBlocking() override;
    void Stop() override;
    void StopBlocking() override;
    void ReadTrace(ReadTraceCallback) override;
    void SetOnStopCallback(std::function<void()>) override;

   private:
    TracingMuxerImpl* const muxer_;
    TracingSessionGlobalID const session_id_;
  };

  struct RegisteredDataSource {
    DataSourceDescriptor descriptor;
    DataSourceFactory factory{};
    DataSourceStaticState* static_state = nullptr;
  };

  struct RegisteredBackend {
    // Backends are supposed to have static lifetime.
    TracingBackend* backend = nullptr;
    TracingBackendId id = 0;
    BackendType type{};

    std::unique_ptr<ProducerImpl> producer;

    // The calling code can request more than one concurrently active tracing
    // session for the same backend. We need to create one consumer per session.
    std::vector<std::unique_ptr<ConsumerImpl>> consumers;
  };

  explicit TracingMuxerImpl(const TracingInitArgs&);
  void Initialize(const TracingInitArgs& args);
  ConsumerImpl* FindConsumer(TracingSessionGlobalID session_id);
  void OnConsumerDisconnected(ConsumerImpl* consumer);

  struct FindDataSourceRes {
    FindDataSourceRes() = default;
    FindDataSourceRes(DataSourceStaticState* a, DataSourceState* b, uint32_t c)
        : static_state(a), internal_state(b), instance_idx(c) {}
    explicit operator bool() const { return !!internal_state; }

    DataSourceStaticState* static_state = nullptr;
    DataSourceState* internal_state = nullptr;
    uint32_t instance_idx = 0;
  };
  FindDataSourceRes FindDataSource(TracingBackendId, DataSourceInstanceID);

  std::unique_ptr<base::TaskRunner> task_runner_;
  std::vector<RegisteredDataSource> data_sources_;
  std::vector<RegisteredBackend> backends_;

  std::atomic<TracingSessionGlobalID> next_tracing_session_id_{};

  PERFETTO_THREAD_CHECKER(thread_checker_)
};

}  // namespace internal
}  // namespace perfetto

#endif  // SRC_TRACING_INTERNAL_TRACING_MUXER_IMPL_H_
