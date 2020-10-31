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

#ifndef INCLUDE_PERFETTO_TRACING_INTERNAL_TRACING_TLS_H_
#define INCLUDE_PERFETTO_TRACING_INTERNAL_TRACING_TLS_H_

#include <array>
#include <memory>

#include "perfetto/tracing/internal/basic_types.h"
#include "perfetto/tracing/internal/data_source_internal.h"
#include "perfetto/tracing/platform.h"

namespace perfetto {

class TraceWriterBase;

namespace internal {

// Organization of the thread-local storage
// ----------------------------------------
// First of all, remember the cardinality of the problem: at any point in time
// there are M data sources registered (i.e. number of subclasses of DataSource)
// and up to N concurrent instances for each data source, so up to M * N total
// data source instances around.
// Each data source instance can be accessed by T threads (no upper bound).
// We can safely put hard limits both to M and N (i.e. say that we support at
// most 32 data source types per process and up to 8 concurrent instances).
//
// We want to make it so from the Platform viewpoint, we use only one global
// TLS object, so T instances in total, one per thread, regardless of M and N.
// This allows to deal with at-thread-exit destruction only in one place, rather
// than N, M or M * N.
//
// Visually:
//                     [    Thread 1   ] [    Thread 2   ] [    Thread T   ]
//                     +---------------+ +---------------+ +---------------+
// Data source Foo     |               | |               | |               |
//  Instance 1         |     TLS       | |     TLS       | |     TLS       |
//  Instance 2         |    Object     | |    Object     | |    Object     |
//  Instance 3         |               | |               | |               |
//                     |               | |               | |               |
// Data source Bar     |               | |               | |               |
//  Instance 1         |               | |               | |               |
//  Instance 2         |               | |               | |               |
//                     +---------------+ +---------------+ +---------------+
//
// Each TLS Object is organized as an array of M DataSourceThreadLocalState.
// Each DSTLS itself is an array of up to N per-instance objects.
// The only per-instance object for now is the TraceWriter.
// So for each data source, for each instance, for each thread we keep one
// TraceWriter.
// The lookup is O(1): Given the TLS object, the TraceWriter is just tls[M][N].
class TracingTLS : public Platform::ThreadLocalObject {
 public:
  ~TracingTLS() override;

  // This is checked against TraceMuxerImpl's global generation counter to
  // handle destruction of TraceWriter(s) that belong to data sources that
  // have been stopped. When the two numbers diverge, a scan of all the
  // thread-local TraceWriter(s) is issued.
  uint32_t generation = 0;

  // By default all data source instances have independent thread-local state
  // (see above).
  std::array<DataSourceThreadLocalState, kMaxDataSources> data_sources_tls{};

  // Track event data sources, however, share the same thread-local state in
  // order to be able to share trace writers and interning state across all
  // track event categories.
  DataSourceThreadLocalState track_event_tls{};
};

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_INTERNAL_TRACING_TLS_H_
