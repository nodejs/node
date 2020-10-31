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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_TOKENIZER_H_

#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

class FtraceTokenizer {
 public:
  explicit FtraceTokenizer(TraceProcessorContext* context)
      : context_(context) {}

  void TokenizeFtraceBundle(TraceBlobView bundle);

 private:
  void TokenizeFtraceEvent(uint32_t cpu, TraceBlobView event);
  void TokenizeFtraceCompactSched(uint32_t cpu,
                                  const uint8_t* data,
                                  size_t size);
  void TokenizeFtraceCompactSchedSwitch(
      uint32_t cpu,
      const protos::pbzero::FtraceEventBundle::CompactSched::Decoder& compact,
      const std::vector<StringId>& string_table);
  void TokenizeFtraceCompactSchedWaking(
      uint32_t cpu,
      const protos::pbzero::FtraceEventBundle::CompactSched::Decoder& compact,
      const std::vector<StringId>& string_table);

  TraceProcessorContext* context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_TOKENIZER_H_
