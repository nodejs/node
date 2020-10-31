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

#ifndef SRC_TRACE_PROCESSOR_TYPES_TRACE_PROCESSOR_CONTEXT_H_
#define SRC_TRACE_PROCESSOR_TYPES_TRACE_PROCESSOR_CONTEXT_H_

#include <memory>
#include <vector>

#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/types/destructible.h"

namespace perfetto {
namespace trace_processor {

class ArgsTracker;
class AndroidProbesTracker;
class ChunkedTraceReader;
class ClockTracker;
class EventTracker;
class ForwardingTraceParser;
class FtraceModule;
class GlobalArgsTracker;
class HeapGraphTracker;
class HeapProfileTracker;
class MetadataTracker;
class PerfSampleTracker;
class ProtoImporterModule;
class ProcessTracker;
class SliceTracker;
class TraceParser;
class TraceSorter;
class TraceStorage;
class TrackTracker;
class JsonTracker;

class TraceProcessorContext {
 public:
  TraceProcessorContext();
  ~TraceProcessorContext();

  Config config;

  std::unique_ptr<TraceStorage> storage;

  std::unique_ptr<ChunkedTraceReader> chunk_reader;
  std::unique_ptr<TraceSorter> sorter;

  // Keep the global tracker before the args tracker as we access the global
  // tracker in the destructor of the args tracker. Also keep it before other
  // trackers, as they may own ArgsTrackers themselves.
  std::unique_ptr<GlobalArgsTracker> global_args_tracker;
  std::unique_ptr<ArgsTracker> args_tracker;

  std::unique_ptr<TrackTracker> track_tracker;
  std::unique_ptr<SliceTracker> slice_tracker;
  std::unique_ptr<ProcessTracker> process_tracker;
  std::unique_ptr<EventTracker> event_tracker;
  std::unique_ptr<ClockTracker> clock_tracker;
  std::unique_ptr<HeapProfileTracker> heap_profile_tracker;
  std::unique_ptr<MetadataTracker> metadata_tracker;
  std::unique_ptr<PerfSampleTracker> perf_sample_tracker;

  // These fields are stored as pointers to Destructible objects rather than
  // their actual type (a subclass of Destructible), as the concrete subclass
  // type is only available in storage_full target. To access these fields use
  // the GetOrCreate() method on their subclass type, e.g.
  // SyscallTracker::GetOrCreate(context)
  std::unique_ptr<Destructible> android_probes_tracker;  // AndroidProbesTracker
  std::unique_ptr<Destructible> syscall_tracker;         // SyscallTracker
  std::unique_ptr<Destructible> sched_tracker;           // SchedEventTracker
  std::unique_ptr<Destructible> binder_tracker;          // BinderTracker
  std::unique_ptr<Destructible> systrace_parser;         // SystraceParser
  std::unique_ptr<Destructible> heap_graph_tracker;      // HeapGraphTracker
  std::unique_ptr<Destructible> json_tracker;            // JsonTracker

  // These fields are trace readers which will be called by |forwarding_parser|
  // once the format of the trace is discovered. They are placed here as they
  // are only available in the storage_full target.
  std::unique_ptr<ChunkedTraceReader> json_trace_tokenizer;
  std::unique_ptr<ChunkedTraceReader> fuchsia_trace_tokenizer;
  std::unique_ptr<ChunkedTraceReader> systrace_trace_parser;
  std::unique_ptr<ChunkedTraceReader> gzip_trace_parser;

  // These fields are trace parsers which will be called by |forwarding_parser|
  // once the format of the trace is discovered. They are placed here as they
  // are only available in the storage_full target.
  std::unique_ptr<TraceParser> json_trace_parser;
  std::unique_ptr<TraceParser> fuchsia_trace_parser;

  // The module at the index N is registered to handle field id N in
  // TracePacket.
  std::vector<ProtoImporterModule*> modules_by_field;
  std::vector<std::unique_ptr<ProtoImporterModule>> modules;
  FtraceModule* ftrace_module = nullptr;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TYPES_TRACE_PROCESSOR_CONTEXT_H_
