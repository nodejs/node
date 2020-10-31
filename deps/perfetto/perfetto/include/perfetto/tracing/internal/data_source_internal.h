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

#ifndef INCLUDE_PERFETTO_TRACING_INTERNAL_DATA_SOURCE_INTERNAL_H_
#define INCLUDE_PERFETTO_TRACING_INTERNAL_DATA_SOURCE_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// No perfetto headers (other than tracing/api and protozero) should be here.
#include "perfetto/tracing/buffer_exhausted_policy.h"
#include "perfetto/tracing/internal/basic_types.h"
#include "perfetto/tracing/trace_writer_base.h"

namespace perfetto {

class DataSourceBase;
class TraceWriterBase;

namespace internal {

class TracingTLS;

// This maintains the internal state of a data source instance that is used only
// to implement the tracing mechanics and is not exposed to the API client.
// There is one of these object per DataSource instance (up to
// kMaxDataSourceInstances).
struct DataSourceState {
  // This boolean flag determines whether the DataSource::Trace() method should
  // do something or be a no-op. This flag doesn't give the full guarantee
  // that tracing data will be visible in the trace, it just makes it so that
  // the client attemps writing trace data and interacting with the service.
  // For instance, when a tracing session ends the service will reject data
  // commits that arrive too late even if the producer hasn't received the stop
  // IPC message.
  // This flag is set right before calling OnStart() and cleared right before
  // calling OnStop(), unless using HandleStopAsynchronously() (see comments
  // in data_source.h).
  // Keep this flag as the first field. This allows the compiler to directly
  // dereference the DataSourceState* pointer in the trace fast-path without
  // doing extra pointr arithmetic.
  bool trace_lambda_enabled = false;

  // The central buffer id that all TraceWriter(s) created by this data source
  // must target.
  BufferId buffer_id = 0;

  // The index within TracingMuxerImpl.backends_. Practically it allows to
  // lookup the Producer object, and hence the IPC channel, for this data
  // source.
  TracingBackendId backend_id = 0;

  // The instance id as assigned by the tracing service. Note that because a
  // process can be connected to >1 services, this ID is not globally unique but
  // is only unique within the scope of its backend.
  // Only the tuple (backend_id, data_source_instance_id) is globally unique.
  uint64_t data_source_instance_id = 0;

  // A hash of the trace config used by this instance. This is used to
  // de-duplicate instances for data sources with identical names (e.g., track
  // event).
  uint64_t config_hash = 0;

  // This lock is not held to implement Trace() and it's used only if the trace
  // code wants to access its own data source state.
  // This is to prevent that accessing the data source on an arbitrary embedder
  // thread races with the internal IPC thread destroying the data source
  // because of a end-of-tracing notification from the service.
  std::recursive_mutex lock;
  std::unique_ptr<DataSourceBase> data_source;
};

// This is to allow lazy-initialization and avoid static initializers and
// at-exit destructors. All the entries are initialized via placement-new when
// DataSource::Register() is called, see TracingMuxerImpl::RegisterDataSource().
struct DataSourceStateStorage {
  alignas(DataSourceState) char storage[sizeof(DataSourceState)]{};
};

// Per-DataSource-type global state.
struct DataSourceStaticState {
  // Unique index of the data source, assigned at registration time.
  uint32_t index = kMaxDataSources;

  // A bitmap that tells about the validity of each |instances| entry. When the
  // i-th bit of the bitmap it's set, instances[i] is valid.
  std::atomic<uint32_t> valid_instances{};
  std::array<DataSourceStateStorage, kMaxDataSourceInstances> instances{};

  // Can be used with a cached |valid_instances| bitmap.
  DataSourceState* TryGetCached(uint32_t cached_bitmap, size_t n) {
    return cached_bitmap & (1 << n)
               ? reinterpret_cast<DataSourceState*>(&instances[n])
               : nullptr;
  }

  DataSourceState* TryGet(size_t n) {
    return TryGetCached(valid_instances.load(std::memory_order_acquire), n);
  }

  void CompilerAsserts() {
    static_assert(sizeof(valid_instances.load()) * 8 >= kMaxDataSourceInstances,
                  "kMaxDataSourceInstances too high");
  }
};

// Per-DataSource-instance thread-local state.
struct DataSourceInstanceThreadLocalState {
  using IncrementalStatePointer = std::unique_ptr<void, void (*)(void*)>;

  void Reset() {
    trace_writer.reset();
    incremental_state.reset();
    backend_id = 0;
    buffer_id = 0;
  }

  std::unique_ptr<TraceWriterBase> trace_writer;
  IncrementalStatePointer incremental_state = {nullptr, [](void*) {}};
  TracingBackendId backend_id;
  BufferId buffer_id;
};

// Per-DataSource-type thread-local state.
struct DataSourceThreadLocalState {
  DataSourceStaticState* static_state = nullptr;

  // Pointer to the parent tls object that holds us. Used to retrieve the
  // generation, which is per-global-TLS and not per data-source.
  TracingTLS* root_tls = nullptr;

  // One entry per each data source instance.
  std::array<DataSourceInstanceThreadLocalState, kMaxDataSourceInstances>
      per_instance{};
};

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_INTERNAL_DATA_SOURCE_INTERNAL_H_
