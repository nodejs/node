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

#ifndef INCLUDE_PERFETTO_EXT_BASE_METATRACE_EVENTS_H_
#define INCLUDE_PERFETTO_EXT_BASE_METATRACE_EVENTS_H_

#include <stdint.h>

namespace perfetto {
namespace metatrace {

enum Tags : uint32_t {
  TAG_NONE = 0,
  TAG_ANY = uint32_t(-1),
  TAG_FTRACE = 1 << 0,
  TAG_PROC_POLLERS = 1 << 1,
  TAG_TRACE_WRITER = 1 << 2,
  TAG_TRACE_SERVICE = 1 << 3,
  TAG_PRODUCER = 1 << 4,
};

// The macros below generate matching enums and arrays of string literals.
// This is to avoid maintaining string maps manually.

// clang-format off

// DO NOT remove or reshuffle items in this list, only append. The ID of these
// events are an ABI, the trace processor relies on these to open old traces.
#define PERFETTO_METATRACE_EVENTS(F) \
  F(EVENT_ZERO_UNUSED), \
  F(FTRACE_CPU_READER_READ), /*unused*/ \
  F(FTRACE_DRAIN_CPUS), /*unused*/ \
  F(FTRACE_UNBLOCK_READERS), /*unused*/ \
  F(FTRACE_CPU_READ_NONBLOCK), /*unused*/ \
  F(FTRACE_CPU_READ_BLOCK), /*unused*/ \
  F(FTRACE_CPU_SPLICE_NONBLOCK), /*unused*/ \
  F(FTRACE_CPU_SPLICE_BLOCK), /*unused*/ \
  F(FTRACE_CPU_WAIT_CMD), /*unused*/ \
  F(FTRACE_CPU_RUN_CYCLE), /*unused*/ \
  F(FTRACE_CPU_FLUSH), \
  F(FTRACE_CPU_DRAIN), /*unused*/ \
  F(READ_SYS_STATS), \
  F(PS_WRITE_ALL_PROCESSES), \
  F(PS_ON_PIDS), \
  F(PS_ON_RENAME_PIDS), \
  F(PS_WRITE_ALL_PROCESS_STATS), \
  F(TRACE_WRITER_COMMIT_STARTUP_WRITER_BATCH), \
  F(FTRACE_READ_TICK), \
  F(FTRACE_CPU_READ_CYCLE), \
  F(FTRACE_CPU_READ_BATCH), \
  F(KALLSYMS_PARSE), \
  F(PROFILER_READ_TICK), \
  F(PROFILER_READ_CPU), \
  F(PROFILER_UNWIND_TICK), \
  F(PROFILER_UNWIND_SAMPLE), \
  F(PROFILER_UNWIND_INITIAL_ATTEMPT), \
  F(PROFILER_UNWIND_ATTEMPT), \
  F(PROFILER_MAPS_PARSE), \
  F(PROFILER_MAPS_REPARSE), \
  F(PROFILER_UNWIND_CACHE_CLEAR)

// Append only, see above.
//
// Values that aren't used as counters:
// * FTRACE_SERVICE_COMMIT_DATA is a bit-packed representation of an event, see
//   tracing_service_impl.cc for the format.
// * PROFILER_UNWIND_CURRENT_PID represents the PID that is being unwound.
//
#define PERFETTO_METATRACE_COUNTERS(F) \
  F(COUNTER_ZERO_UNUSED),\
  F(FTRACE_PAGES_DRAINED), \
  F(PS_PIDS_SCANNED), \
  F(TRACE_SERVICE_COMMIT_DATA), \
  F(PROFILER_UNWIND_QUEUE_SZ), \
  F(PROFILER_UNWIND_CURRENT_PID)

// clang-format on

#define PERFETTO_METATRACE_IDENTITY(name) name
#define PERFETTO_METATRACE_TOSTRING(name) #name

enum Events : uint16_t {
  PERFETTO_METATRACE_EVENTS(PERFETTO_METATRACE_IDENTITY),
  EVENTS_MAX
};
constexpr char const* kEventNames[] = {
    PERFETTO_METATRACE_EVENTS(PERFETTO_METATRACE_TOSTRING)};

enum Counters : uint16_t {
  PERFETTO_METATRACE_COUNTERS(PERFETTO_METATRACE_IDENTITY),
  COUNTERS_MAX
};
constexpr char const* kCounterNames[] = {
    PERFETTO_METATRACE_COUNTERS(PERFETTO_METATRACE_TOSTRING)};

inline void SuppressUnusedVarsInAmalgamatedBuild() {
  (void)kCounterNames;
  (void)kEventNames;
}

}  // namespace metatrace
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_METATRACE_EVENTS_H_
