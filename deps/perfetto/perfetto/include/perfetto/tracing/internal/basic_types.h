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

#ifndef INCLUDE_PERFETTO_TRACING_INTERNAL_BASIC_TYPES_H_
#define INCLUDE_PERFETTO_TRACING_INTERNAL_BASIC_TYPES_H_

#include <stddef.h>
#include <stdint.h>

namespace perfetto {
namespace internal {

// A static_assert in tracing_muxer_impl.cc guarantees that this stays in sync
// with the definition in tracing/core/basic_types.h
using BufferId = uint16_t;

// This is a direct index in the TracingMuxer::backends_ vector.
// Backends are only added and never removed.
using TracingBackendId = size_t;

// Max numbers of data sources that can be registered in a process.
constexpr size_t kMaxDataSources = 32;

// Max instances for each data source type. This typically matches the
// "max number of concurrent tracing sessions". However remember that a data
// source can be instantiated more than once within one tracing session by
// creating two entries for it in the trace config.
constexpr size_t kMaxDataSourceInstances = 8;

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_INTERNAL_BASIC_TYPES_H_
