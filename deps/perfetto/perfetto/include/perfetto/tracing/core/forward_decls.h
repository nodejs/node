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

#ifndef INCLUDE_PERFETTO_TRACING_CORE_FORWARD_DECLS_H_
#define INCLUDE_PERFETTO_TRACING_CORE_FORWARD_DECLS_H_

// Forward declares classes that are generated at build-time from protos.
// First of all, why are we forward declaring at all?
//  1. Chromium diverges from the Google style guide on this, because forward
//     declarations typically make build times faster, and that's a desirable
//     property for a large and complex codebase.
//  2. Adding #include to build-time-generated headers from headers typically
//     creates subtle build errors that are hard to spot in GN. This is because
//     once a standard header (say foo.h) has an #include "protos/foo.gen.h",
//     the build target that depends on foo.h needs to depend on the genrule
//     that generates foo.gen.h. This is achievable using public_deps in GN but
//     is not testable / enforceable, hence too easy to get wrong.

// Historically the classes below used to be generated from the corresponding
// .proto(s) at CL *check-in* time (!= build time) in the ::perfetto namespace.
// Nowadays we have code everywhere that assume the right class is
// ::perfetto::TraceConfig or the like. Back then other headers could just
// forward declared ::perfetto::TraceConfig. These days, the real class is
// ::perfetto::protos::gen::TraceConfig and core/trace_config.h aliases that as
// using ::perfetto::TraceConfig = ::perfetto::protos::gen::TraceConfig.
// In C++ one cannot forward declare a type alias (but only the aliased type).
// Hence this header, which should be used every time one wants to forward
// declare classes like TraceConfig.

// The overall plan is that, when one of the classes below is needed:
// The .h file includes this file.
// The .cc file includes perfetto/tracing/core/trace_config.h (or equiv). That
// header will pull the full declaration from trace_config.gen.h and will also
// setup the alias in the ::perfetto namespace.

namespace perfetto {
namespace protos {
namespace gen {

class ChromeConfig;
class CommitDataRequest;
class DataSourceConfig;
class DataSourceDescriptor;
class ObservableEvents;
class TraceConfig;
class TraceStats;
class TracingServiceCapabilities;
class TracingServiceState;

}  // namespace gen
}  // namespace protos

using ChromeConfig = ::perfetto::protos::gen::ChromeConfig;
using CommitDataRequest = ::perfetto::protos::gen::CommitDataRequest;
using DataSourceConfig = ::perfetto::protos::gen::DataSourceConfig;
using DataSourceDescriptor = ::perfetto::protos::gen::DataSourceDescriptor;
using ObservableEvents = ::perfetto::protos::gen::ObservableEvents;
using TraceConfig = ::perfetto::protos::gen::TraceConfig;
using TraceStats = ::perfetto::protos::gen::TraceStats;
using TracingServiceCapabilities =
    ::perfetto::protos::gen::TracingServiceCapabilities;
using TracingServiceState = ::perfetto::protos::gen::TracingServiceState;

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_CORE_FORWARD_DECLS_H_
