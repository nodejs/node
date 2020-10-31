/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_ARBITER_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_ARBITER_H_

#include <stddef.h>

#include <functional>
#include <memory>
#include <vector>

#include "perfetto/base/export.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/buffer_exhausted_policy.h"

namespace perfetto {

namespace base {
class TaskRunner;
}

class SharedMemory;
class TraceWriter;

// Used by the Producer-side of the transport layer to vend TraceWriters
// from the SharedMemory it receives from the Service-side.
class PERFETTO_EXPORT SharedMemoryArbiter {
 public:
  virtual ~SharedMemoryArbiter();

  // Creates a new TraceWriter and assigns it a new WriterID. The WriterID is
  // written in each chunk header owned by a given TraceWriter and is used by
  // the Service to reconstruct TracePackets written by the same TraceWriter.
  // Returns null impl of TraceWriter if all WriterID slots are exhausted. The
  // writer will commit to the provided |target_buffer|. If the arbiter was
  // created via CreateUnbound(), only BufferExhaustedPolicy::kDrop is
  // supported.
  virtual std::unique_ptr<TraceWriter> CreateTraceWriter(
      BufferID target_buffer,
      BufferExhaustedPolicy buffer_exhausted_policy =
          BufferExhaustedPolicy::kDefault) = 0;

  // Creates a TraceWriter that will commit to the target buffer with the given
  // reservation ID (creating a new reservation for this ID if none exists yet).
  // The buffer reservation should be bound to an actual BufferID via
  // BindStartupTargetBuffer() once the actual BufferID is known. Only supported
  // if the arbiter was created using CreateUnbound(), and may be called while
  // the arbiter is unbound.
  //
  // While any unbound buffer reservation exists, all commits will be buffered
  // until all reservations were bound. Thus, until all reservations are bound,
  // the data written to the SMB will not be consumed by the service - the SMB
  // size should be chosen with this in mind. Startup writers always use
  // BufferExhaustedPolicy::kDrop, as we cannot feasibly stall while not
  // flushing to the service.
  //
  // The |target_buffer_reservation_id| should be greater than 0 but can
  // otherwise be freely chosen by the producer and is only used to translate
  // packets into the actual buffer id once
  // BindStartupTargetBuffer(reservation_id) is called. For example, Chrome uses
  // startup tracing not only for the first, but also subsequent tracing
  // sessions (to enable tracing in the browser process before it instructs the
  // tracing service to start tracing asynchronously, minimizing trace data loss
  // in the meantime), and increments the reservation ID between sessions.
  // Similarly, if more than a single target buffer per session is required
  // (e.g. for two different data sources), different reservation IDs should be
  // chosen for different targets buffers.
  virtual std::unique_ptr<TraceWriter> CreateStartupTraceWriter(
      uint16_t target_buffer_reservation_id) = 0;

  // Should only be called on unbound SharedMemoryArbiters. Binds the arbiter to
  // the provided ProducerEndpoint and TaskRunner. Should be called only once
  // and on the provided |TaskRunner|. Usually called by the producer (i.e., no
  // specific data source) once it connects to the service. Both the endpoint
  // and task runner should remain valid for the remainder of the arbiter's
  // lifetime.
  virtual void BindToProducerEndpoint(TracingService::ProducerEndpoint*,
                                      base::TaskRunner*) = 0;

  // Binds commits from TraceWriters created via CreateStartupTraceWriter() with
  // the given |target_buffer_reservation_id| to |target_buffer_id|. May only be
  // called once per |target_buffer_reservation_id|. Should be called on the
  // arbiter's TaskRunner, and after BindToProducerEndpoint() was called.
  // Usually, it is called by a specific data source, after it received its
  // configuration (including the target buffer ID) from the service.
  virtual void BindStartupTargetBuffer(uint16_t target_buffer_reservation_id,
                                       BufferID target_buffer_id) = 0;

  // Treat the reservation as resolved to an invalid buffer. Commits for this
  // reservation will be flushed to the service ASAP. The service will free
  // committed chunks but otherwise ignore them. The producer can call this
  // method, for example, if connection to the tracing service failed or the
  // session was stopped concurrently before the connection was established.
  virtual void AbortStartupTracingForReservation(
      uint16_t target_buffer_reservation_id) = 0;

  // Notifies the service that all data for the given FlushRequestID has been
  // committed in the shared memory buffer. Should only be called while bound.
  virtual void NotifyFlushComplete(FlushRequestID) = 0;

  // Create a bound arbiter instance. Args:
  // |SharedMemory|: the shared memory buffer to use.
  // |page_size|: a multiple of 4KB that defines the granularity of tracing
  // pages. See tradeoff considerations in shared_memory_abi.h.
  // |ProducerEndpoint|: The service's producer endpoint used e.g. to commit
  // chunks and register trace writers.
  // |TaskRunner|: Task runner for perfetto's main thread, which executes the
  // OnPagesCompleteCallback and IPC calls to the |ProducerEndpoint|.
  //
  // Implemented in src/core/shared_memory_arbiter_impl.cc.
  static std::unique_ptr<SharedMemoryArbiter> CreateInstance(
      SharedMemory*,
      size_t page_size,
      TracingService::ProducerEndpoint*,
      base::TaskRunner*);

  // Create an unbound arbiter instance, which should later be bound to a
  // ProducerEndpoint and TaskRunner by calling BindToProducerEndpoint(). The
  // returned arbiter will ONLY support trace writers with
  // BufferExhaustedPolicy::kDrop.
  //
  // An unbound SharedMemoryArbiter can be used to write to a producer-created
  // SharedMemory buffer before the producer connects to the tracing service.
  // The producer can then pass this SMB to the service when it connects (see
  // TracingService::ConnectProducer).
  //
  // To trace into the SMB before the service starts the tracing session, trace
  // writers can be obtained via CreateStartupTraceWriter() and later associated
  // with a target buffer via BindStartupTargetBuffer(), once the target buffer
  // is known.
  //
  // Implemented in src/core/shared_memory_arbiter_impl.cc. See CreateInstance()
  // for comments about the arguments.
  static std::unique_ptr<SharedMemoryArbiter> CreateUnboundInstance(
      SharedMemory*,
      size_t page_size);
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_ARBITER_H_
