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

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_PRODUCER_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_PRODUCER_H_

#include "perfetto/base/export.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/tracing/core/forward_decls.h"
namespace perfetto {

class SharedMemory;

// A Producer is an entity that connects to the write-only port of the Service
// and exposes the ability to produce performance data on-demand. The lifecycle
// of a Producer is as follows:
// 1. The producer connects to the service and advertises its data sources
//    (e.g., the ability to get kernel ftraces, to list process stats).
// 2. The service acknowledges the connection and sends over the SharedMemory
//    region that will be used to exchange data (together with the signalling
//    API TracingService::ProducerEndpoint::OnPageAcquired()/OnPageReleased()).
// 3. At some point later on, the Service asks the Producer to turn on some of
//    the previously registered data sources, together with some configuration
//    parameters. This happens via the StartDataSource() callback.
// 4. In response to that the Producer will spawn an instance of the given data
//    source and inject its data into the shared memory buffer (obtained during
//    OnConnect).
// This interface is subclassed by:
//  1. The actual producer code in the clients e.g., the ftrace reader process.
//  2. The transport layer when interposing RPC between service and producers.
class PERFETTO_EXPORT Producer {
 public:
  virtual ~Producer();

  // Called by Service (or more typically by the transport layer, on behalf of
  // the remote Service), once the Producer <> Service connection has been
  // established.
  virtual void OnConnect() = 0;

  // Called by the Service or by the transport layer if the connection with the
  // service drops, either voluntarily (e.g., by destroying the ProducerEndpoint
  // obtained through Service::ConnectProducer()) or involuntarily (e.g., if the
  // Service process crashes).
  // The Producer is expected to tear down all its data sources if this happens.
  // Once this call returns it is possible to safely destroy the Producer
  // instance.
  virtual void OnDisconnect() = 0;

  // Called by the Service after OnConnect but before the first DataSource is
  // created. Can be used for any setup required before tracing begins.
  virtual void OnTracingSetup() = 0;

  // The lifecycle methods below are always called in the following sequence:
  // SetupDataSource  -> StartDataSource -> StopDataSource.
  // Or, in the edge case where a trace is aborted immediately:
  // SetupDataSource  -> StopDataSource.
  // The Setup+Start call sequence is always guaranateed, regardless of the
  // TraceConfig.deferred_start flags.
  // Called by the Service to configure one of the data sources previously
  // registered through TracingService::ProducerEndpoint::RegisterDataSource().
  // This method is always called before StartDataSource. There is always a
  // SetupDataSource() call before each StartDataSource() call.
  // Args:
  // - DataSourceInstanceID is an identifier chosen by the Service that should
  //   be assigned to the newly created data source instance. It is used to
  //   match the StopDataSource() request below.
  // - DataSourceConfig is the configuration for the new data source (e.g.,
  //   tells which trace categories to enable).
  virtual void SetupDataSource(DataSourceInstanceID,
                               const DataSourceConfig&) = 0;

  // Called by the Service to turn on one of the data sources previously
  // registered through TracingService::ProducerEndpoint::RegisterDataSource()
  // and initialized through SetupDataSource().
  // Both arguments are guaranteed to be identical to the ones passed to the
  // prior SetupDataSource() call.
  virtual void StartDataSource(DataSourceInstanceID,
                               const DataSourceConfig&) = 0;

  // Called by the Service to shut down an existing data source instance.
  virtual void StopDataSource(DataSourceInstanceID) = 0;

  // Called by the service to request the Producer to commit the data of the
  // given data sources and return their chunks into the shared memory buffer.
  // The Producer is expected to invoke NotifyFlushComplete(FlushRequestID) on
  // the Service after the data has been committed. The producer has to either
  // reply to the flush requests in order, or can just reply to the latest one
  // Upon seeing a NotifyFlushComplete(N), the service will assume that all
  // flushes < N have also been committed.
  virtual void Flush(FlushRequestID,
                     const DataSourceInstanceID* data_source_ids,
                     size_t num_data_sources) = 0;

  // Called by the service to instruct the given data sources to stop referring
  // to any trace contents emitted so far. The intent is that after processing
  // this call, the rest of the trace should be parsable even if all of the
  // packets emitted so far have been lost (for example due to ring buffer
  // overwrites).
  //
  // Called only for Producers with active data sources that have opted in by
  // setting |handles_incremental_state_clear| in their DataSourceDescriptor.
  //
  // The way this call is handled is up to the individual Producer
  // implementation. Some might wish to emit invalidation markers in the trace
  // (see TracePacket.incremental_state_cleared for an existing field), and
  // handle them when parsing the trace.
  virtual void ClearIncrementalState(
      const DataSourceInstanceID* data_source_ids,
      size_t num_data_sources) = 0;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_PRODUCER_H_
