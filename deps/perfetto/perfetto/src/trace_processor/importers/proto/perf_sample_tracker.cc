/*
 * Copyright (C) 2020 The Android Open Source Project
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
#include "src/trace_processor/importers/proto/perf_sample_tracker.h"

#include <vector>

#include <inttypes.h>

#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <cxxabi.h>
#endif

namespace perfetto {
namespace trace_processor {
namespace {
// TODO(rsavitski): consider using the sampling rate from the trace config.
constexpr int64_t kFixedStackSliceDurationNs = 1 * 1000 * 1000;
}  // namespace

void PerfSampleTracker::AddStackToSliceTrack(int64_t timestamp,
                                             CallsiteId leaf_id,
                                             uint32_t pid,
                                             uint32_t tid,
                                             uint32_t cpu) {
  UniquePid upid =
      context_->process_tracker->GetOrCreateProcess(static_cast<uint32_t>(pid));

  TrackId track_id = context_->track_tracker->InternPerfStackTrack(upid);
  const auto& callsites = context_->storage->stack_profile_callsite_table();
  const auto& frames = context_->storage->stack_profile_frame_table();
  const auto& mappings = context_->storage->stack_profile_mapping_table();

  // Synthetic frame for more context, as the track is process-wide.
  char buf[128] = {};
  snprintf(buf, sizeof(buf), "cpu: [%" PRIu32 "]; thread: [%" PRIi32 "]", cpu,
           tid);
  StringId synth = context_->storage->InternString(buf);
  context_->slice_tracker->Scoped(timestamp, track_id, kNullStringId, synth,
                                  kFixedStackSliceDurationNs);

  // The callstack id references the leaf frame, while we want the slice stack
  // to have the root frame at the top in the UI, so walk the chain in reverse.
  std::vector<uint32_t> callsite_rows;
  callsite_rows.reserve(64);
  base::Optional<CallsiteId> cs_id = leaf_id;
  while (cs_id) {
    uint32_t row = *callsites.id().IndexOf(*cs_id);
    callsite_rows.push_back(row);
    cs_id = callsites.parent_id()[row];
  }

  for (auto rit = callsite_rows.rbegin(); rit != callsite_rows.rend(); ++rit) {
    uint32_t callsite_row = *rit;
    FrameId frame_id = callsites.frame_id()[callsite_row];
    uint32_t frame_row = *frames.id().IndexOf(frame_id);

    MappingId mapping_id = frames.mapping()[frame_row];
    uint32_t mapping_row = *mappings.id().IndexOf(mapping_id);

    StringId mangled_fname = frames.name()[frame_row];
    StringId mname = mappings.name()[mapping_row];

    StringId fname = MaybeDemangle(mangled_fname);
    context_->slice_tracker->Scoped(timestamp, track_id, mname, fname,
                                    kFixedStackSliceDurationNs);
  }
}

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
StringId PerfSampleTracker::MaybeDemangle(StringId fname) {
  StringId ret = fname;
  const char* raw_fname = context_->storage->GetString(fname).c_str();
  int ignored;
  char* data = abi::__cxa_demangle(raw_fname, nullptr, nullptr, &ignored);
  if (data) {
    ret = context_->storage->InternString(data);
    free(data);
  }
  return ret;
}
#else
StringId PerfSampleTracker::MaybeDemangle(StringId fname) {
  return fname;
}
#endif

}  // namespace trace_processor
}  // namespace perfetto
