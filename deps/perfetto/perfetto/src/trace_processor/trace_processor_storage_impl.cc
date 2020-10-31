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

#include "src/trace_processor/trace_processor_storage_impl.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/forwarding_trace_parser.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/default_modules.h"
#include "src/trace_processor/importers/proto/heap_profile_tracker.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"
#include "src/trace_processor/importers/proto/perf_sample_tracker.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/importers/proto/proto_trace_tokenizer.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_sorter.h"

namespace perfetto {
namespace trace_processor {

TraceProcessorStorageImpl::TraceProcessorStorageImpl(const Config& cfg) {
  context_.config = cfg;
  context_.storage.reset(new TraceStorage(context_.config));
  context_.track_tracker.reset(new TrackTracker(&context_));
  context_.args_tracker.reset(new ArgsTracker(&context_));
  context_.slice_tracker.reset(new SliceTracker(&context_));
  context_.event_tracker.reset(new EventTracker(&context_));
  context_.process_tracker.reset(new ProcessTracker(&context_));
  context_.clock_tracker.reset(new ClockTracker(&context_));
  context_.heap_profile_tracker.reset(new HeapProfileTracker(&context_));
  context_.metadata_tracker.reset(new MetadataTracker(&context_));
  context_.global_args_tracker.reset(new GlobalArgsTracker(&context_));
  context_.perf_sample_tracker.reset(new PerfSampleTracker(&context_));

  RegisterDefaultModules(&context_);
}

TraceProcessorStorageImpl::~TraceProcessorStorageImpl() {}

util::Status TraceProcessorStorageImpl::Parse(std::unique_ptr<uint8_t[]> data,
                                              size_t size) {
  if (size == 0)
    return util::OkStatus();
  if (unrecoverable_parse_error_)
    return util::ErrStatus(
        "Failed unrecoverably while parsing in a previous Parse call");
  if (!context_.chunk_reader)
    context_.chunk_reader.reset(new ForwardingTraceParser(&context_));

  auto scoped_trace = context_.storage->TraceExecutionTimeIntoStats(
      stats::parse_trace_duration_ns);
  util::Status status = context_.chunk_reader->Parse(std::move(data), size);
  unrecoverable_parse_error_ |= !status.ok();
  return status;
}

void TraceProcessorStorageImpl::NotifyEndOfFile() {
  if (unrecoverable_parse_error_ || !context_.chunk_reader)
    return;

  context_.chunk_reader->NotifyEndOfFile();
  if (context_.sorter)
    context_.sorter->ExtractEventsForced();
  context_.event_tracker->FlushPendingEvents();
  context_.slice_tracker->FlushPendingSlices();
  context_.heap_profile_tracker->NotifyEndOfFile();
  for (std::unique_ptr<ProtoImporterModule>& module : context_.modules) {
    module->NotifyEndOfFile();
  }
}

}  // namespace trace_processor
}  // namespace perfetto
