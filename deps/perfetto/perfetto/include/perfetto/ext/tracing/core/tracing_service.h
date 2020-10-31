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

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_TRACING_SERVICE_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_TRACING_SERVICE_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <vector>

#include "perfetto/base/export.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "perfetto/tracing/buffer_exhausted_policy.h"
#include "perfetto/tracing/core/forward_decls.h"

namespace perfetto {

namespace base {
class TaskRunner;
}  // namespace base

class Consumer;
class Producer;
class SharedMemoryArbiter;
class TraceWriter;

// TODO: for the moment this assumes that all the calls happen on the same
// thread/sequence. Not sure this will be the case long term in Chrome.

// The API for the Producer port of the Service.
// Subclassed by:
// 1. The tracing_service_impl.cc business logic when returning it in response
//    to the ConnectProducer() method.
// 2. The transport layer (e.g., src/ipc) when the producer and
//    the service don't talk locally but via some IPC mechanism.
class PERFETTO_EXPORT ProducerEndpoint {
 public:
  virtual ~ProducerEndpoint();

  // Called by the Producer to (un)register data sources. Data sources are
  // identified by their name (i.e. DataSourceDescriptor.name)
  virtual void RegisterDataSource(const DataSourceDescriptor&) = 0;
  virtual void UnregisterDataSource(const std::string& name) = 0;

  // Associate the trace writer with the given |writer_id| with
  // |target_buffer|. The service may use this information to retrieve and
  // copy uncommitted chunks written by the trace writer into its associated
  // buffer, e.g. when a producer process crashes or when a flush is
  // necessary.
  virtual void RegisterTraceWriter(uint32_t writer_id,
                                   uint32_t target_buffer) = 0;

  // Remove the association of the trace writer previously created via
  // RegisterTraceWriter.
  virtual void UnregisterTraceWriter(uint32_t writer_id) = 0;

  // Called by the Producer to signal that some pages in the shared memory
  // buffer (shared between Service and Producer) have changed.
  // When the Producer and the Service are hosted in the same process and
  // hence potentially live on the same task runner, This method must call
  // TracingServiceImpl's CommitData synchronously, without any PostTask()s,
  // if on the same thread. This is to avoid a deadlock where the Producer
  // exhausts its SMB and stalls waiting for the service to catch up with
  // reads, but the Service never gets to that because it lives on the same
  // thread.
  using CommitDataCallback = std::function<void()>;
  virtual void CommitData(const CommitDataRequest&,
                          CommitDataCallback callback = {}) = 0;

  virtual SharedMemory* shared_memory() const = 0;

  // Size of shared memory buffer pages. It's always a multiple of 4K.
  // See shared_memory_abi.h
  virtual size_t shared_buffer_page_size_kb() const = 0;

  // Creates a trace writer, which allows to create events, handling the
  // underying shared memory buffer and signalling to the Service. This method
  // is thread-safe but the returned object is not. A TraceWriter should be
  // used only from a single thread, or the caller has to handle sequencing
  // via a mutex or equivalent. This method can only be called if
  // TracingService::ConnectProducer was called with |in_process=true|.
  // Args:
  // |target_buffer| is the target buffer ID where the data produced by the
  // writer should be stored by the tracing service. This value is passed
  // upon creation of the data source (StartDataSource()) in the
  // DataSourceConfig.target_buffer().
  virtual std::unique_ptr<TraceWriter> CreateTraceWriter(
      BufferID target_buffer,
      BufferExhaustedPolicy buffer_exhausted_policy =
          BufferExhaustedPolicy::kDefault) = 0;

  // TODO(eseckler): Also expose CreateStartupTraceWriter() ?

  // In some cases you can access the producer's SharedMemoryArbiter (for
  // example if TracingService::ConnectProducer is called with
  // |in_process=true|). The SharedMemoryArbiter can be used to create
  // TraceWriters which is able to directly commit chunks. For the
  // |in_process=true| case this can be done without going through an IPC layer.
  virtual SharedMemoryArbiter* MaybeSharedMemoryArbiter() = 0;

  // Whether the service accepted a shared memory buffer provided by the
  // producer.
  virtual bool IsShmemProvidedByProducer() const = 0;

  // Called in response to a Producer::Flush(request_id) call after all data
  // for the flush request has been committed.
  virtual void NotifyFlushComplete(FlushRequestID) = 0;

  // Called in response to one or more Producer::StartDataSource(),
  // if the data source registered setting the flag
  // DataSourceDescriptor.will_notify_on_start.
  virtual void NotifyDataSourceStarted(DataSourceInstanceID) = 0;

  // Called in response to one or more Producer::StopDataSource(),
  // if the data source registered setting the flag
  // DataSourceDescriptor.will_notify_on_stop.
  virtual void NotifyDataSourceStopped(DataSourceInstanceID) = 0;

  // This informs the service to activate any of these triggers if any tracing
  // session was waiting for them.
  virtual void ActivateTriggers(const std::vector<std::string>&) = 0;

  // Emits a synchronization barrier to linearize with the service. When
  // |callback| is invoked, the caller has the guarantee that the service has
  // seen and processed all the requests sent by this producer prior to the
  // Sync() call. Used mainly in tests.
  virtual void Sync(std::function<void()> callback) = 0;
};  // class ProducerEndpoint.

// The API for the Consumer port of the Service.
// Subclassed by:
// 1. The tracing_service_impl.cc business logic when returning it in response
// to
//    the ConnectConsumer() method.
// 2. The transport layer (e.g., src/ipc) when the consumer and
//    the service don't talk locally but via some IPC mechanism.
class ConsumerEndpoint {
 public:
  virtual ~ConsumerEndpoint();

  // Enables tracing with the given TraceConfig. The ScopedFile argument is
  // used only when TraceConfig.write_into_file == true.
  // If TraceConfig.deferred_start == true data sources are configured via
  // SetupDataSource() but are not started until StartTracing() is called.
  // This is to support pre-initialization and fast triggering of traces.
  // The ScopedFile argument is used only when TraceConfig.write_into_file
  // == true.
  virtual void EnableTracing(const TraceConfig&,
                             base::ScopedFile = base::ScopedFile()) = 0;

  // Update the trace config of an existing tracing session; only a subset
  // of options can be changed mid-session. Currently the only
  // supported functionality is expanding the list of producer_name_filters()
  // (or removing the filter entirely) for existing data sources.
  virtual void ChangeTraceConfig(const TraceConfig&) = 0;

  // Starts all data sources configured in the trace config. This is used only
  // after calling EnableTracing() with TraceConfig.deferred_start=true.
  // It's a no-op if called after a regular EnableTracing(), without setting
  // deferred_start.
  virtual void StartTracing() = 0;

  virtual void DisableTracing() = 0;

  // Requests all data sources to flush their data immediately and invokes the
  // passed callback once all of them have acked the flush (in which case
  // the callback argument |success| will be true) or |timeout_ms| are elapsed
  // (in which case |success| will be false).
  // If |timeout_ms| is 0 the TraceConfig's flush_timeout_ms is used, or,
  // if that one is not set (or is set to 0), kDefaultFlushTimeoutMs (5s) is
  // used.
  using FlushCallback = std::function<void(bool /*success*/)>;
  virtual void Flush(uint32_t timeout_ms, FlushCallback) = 0;

  // Tracing data will be delivered invoking Consumer::OnTraceData().
  virtual void ReadBuffers() = 0;

  virtual void FreeBuffers() = 0;

  // Will call OnDetach().
  virtual void Detach(const std::string& key) = 0;

  // Will call OnAttach().
  virtual void Attach(const std::string& key) = 0;

  // Will call OnTraceStats().
  virtual void GetTraceStats() = 0;

  // Start or stop observing events of selected types. |events_mask| specifies
  // the types of events to observe in a bitmask of ObservableEvents::Type.
  // To disable observing, pass 0.
  // Will call OnObservableEvents() repeatedly whenever an event of an enabled
  // ObservableEventType occurs.
  // TODO(eseckler): Extend this to support producers & data sources.
  virtual void ObserveEvents(uint32_t events_mask) = 0;

  // Used to obtain the list of connected data sources and other info about
  // the tracing service.
  using QueryServiceStateCallback =
      std::function<void(bool success, const TracingServiceState&)>;
  virtual void QueryServiceState(QueryServiceStateCallback) = 0;

  // Used for feature detection. Makes sense only when the consumer and the
  // service talk over IPC and can be from different versions.
  using QueryCapabilitiesCallback =
      std::function<void(const TracingServiceCapabilities&)>;
  virtual void QueryCapabilities(QueryCapabilitiesCallback) = 0;
};  // class ConsumerEndpoint.

// The public API of the tracing Service business logic.
//
// Exposed to:
// 1. The transport layer (e.g., src/unix_rpc/unix_service_host.cc),
//    which forwards commands received from a remote producer or consumer to
//    the actual service implementation.
// 2. Tests.
//
// Subclassed by:
//   The service business logic in src/core/tracing_service_impl.cc.
class PERFETTO_EXPORT TracingService {
 public:
  using ProducerEndpoint = perfetto::ProducerEndpoint;
  using ConsumerEndpoint = perfetto::ConsumerEndpoint;

  enum class ProducerSMBScrapingMode {
    // Use service's default setting for SMB scraping. Currently, the default
    // mode is to disable SMB scraping, but this may change in the future.
    kDefault,

    // Enable scraping of uncommitted chunks in producers' shared memory
    // buffers.
    kEnabled,

    // Disable scraping of uncommitted chunks in producers' shared memory
    // buffers.
    kDisabled
  };

  // Implemented in src/core/tracing_service_impl.cc .
  static std::unique_ptr<TracingService> CreateInstance(
      std::unique_ptr<SharedMemory::Factory>,
      base::TaskRunner*);

  virtual ~TracingService();

  // Connects a Producer instance and obtains a ProducerEndpoint, which is
  // essentially a 1:1 channel between one Producer and the Service.
  //
  // The caller has to guarantee that the passed Producer will be alive as long
  // as the returned ProducerEndpoint is alive. Both the passed Producer and the
  // returned ProducerEndpoint must live on the same task runner of the service,
  // specifically:
  // 1) The Service will call Producer::* methods on the Service's task runner.
  // 2) The Producer should call ProducerEndpoint::* methods only on the
  //    service's task runner, except for ProducerEndpoint::CreateTraceWriter(),
  //    which can be called on any thread. To disconnect just destroy the
  //    returned ProducerEndpoint object. It is safe to destroy the Producer
  //    once the Producer::OnDisconnect() has been invoked.
  //
  // |uid| is the trusted user id of the producer process, used by the consumers
  // for validating the origin of trace data. |shared_memory_size_hint_bytes|
  // and |shared_memory_page_size_hint_bytes| are optional hints on the size of
  // the shared memory buffer and its pages. The service can ignore the hints
  // (e.g., if the hints are unreasonably large or other sizes were configured
  // in a tracing session's config). |in_process| enables the ProducerEndpoint
  // to manage its own shared memory and enables use of
  // |ProducerEndpoint::CreateTraceWriter|.
  //
  // The producer can optionally provide a non-null |shm|, which the service
  // will adopt for the connection to the producer, provided it is correctly
  // sized. In this case, |shared_memory_page_size_hint_bytes| indicates the
  // page size used in this SMB. The producer can use this mechanism to record
  // tracing data to an SMB even before the tracing session is started by the
  // service. This is used in Chrome to implement startup tracing. If the buffer
  // is incorrectly sized, the service will discard the SMB and allocate a new
  // one, provided to the producer via ProducerEndpoint::shared_memory() after
  // OnTracingSetup(). To verify that the service accepted the SMB, the producer
  // may check via ProducerEndpoint::IsShmemProvidedByProducer(). If the service
  // accepted the SMB, the producer can then commit any data that is already in
  // the SMB after the tracing session was started by the service via
  // Producer::StartDataSource(). The |shm| will also be rejected when
  // connecting to a service that is too old (pre Android-11).
  //
  // Can return null in the unlikely event that service has too many producers
  // connected.
  virtual std::unique_ptr<ProducerEndpoint> ConnectProducer(
      Producer*,
      uid_t uid,
      const std::string& name,
      size_t shared_memory_size_hint_bytes = 0,
      bool in_process = false,
      ProducerSMBScrapingMode smb_scraping_mode =
          ProducerSMBScrapingMode::kDefault,
      size_t shared_memory_page_size_hint_bytes = 0,
      std::unique_ptr<SharedMemory> shm = nullptr) = 0;

  // Connects a Consumer instance and obtains a ConsumerEndpoint, which is
  // essentially a 1:1 channel between one Consumer and the Service.
  // The caller has to guarantee that the passed Consumer will be alive as long
  // as the returned ConsumerEndpoint is alive.
  // To disconnect just destroy the returned ConsumerEndpoint object. It is safe
  // to destroy the Consumer once the Consumer::OnDisconnect() has been invoked.
  virtual std::unique_ptr<ConsumerEndpoint> ConnectConsumer(Consumer*,
                                                            uid_t) = 0;

  // Enable/disable scraping of chunks in the shared memory buffer. If enabled,
  // the service will copy uncommitted but non-empty chunks from the SMB when
  // flushing (e.g. to handle unresponsive producers or producers unable to
  // flush their active chunks), on producer disconnect (e.g. to recover data
  // from crashed producers), and after disabling a tracing session (e.g. to
  // gather data from producers that didn't stop their data sources in time).
  //
  // This feature is currently used by Chrome.
  virtual void SetSMBScrapingEnabled(bool enabled) = 0;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_TRACING_SERVICE_H_
