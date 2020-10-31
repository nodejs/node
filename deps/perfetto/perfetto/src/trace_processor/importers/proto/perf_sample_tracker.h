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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_

#include <stdint.h>

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class PerfSampleTracker {
 public:
  explicit PerfSampleTracker(TraceProcessorContext* context)
      : context_(context) {}

  // Interim UI track for visualizing stack samples as stacks of slices.
  void AddStackToSliceTrack(int64_t timestamp,
                            CallsiteId leaf_id,
                            uint32_t pid,
                            uint32_t tid,
                            uint32_t cpu);

 private:
  StringId MaybeDemangle(StringId original);

  TraceProcessorContext* const context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_
