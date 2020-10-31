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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FUCHSIA_FUCHSIA_TRACE_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FUCHSIA_FUCHSIA_TRACE_TOKENIZER_H_

#include "src/trace_processor/chunked_trace_reader.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_utils.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_blob_view.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// The Fuchsia trace format is documented at
// https://fuchsia.googlesource.com/fuchsia/+/HEAD/docs/development/tracing/trace-format/README.md
class FuchsiaTraceTokenizer : public ChunkedTraceReader {
 public:
  explicit FuchsiaTraceTokenizer(TraceProcessorContext*);
  ~FuchsiaTraceTokenizer() override;

  // ChunkedTraceReader implementation
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) override;
  void NotifyEndOfFile() override;

 private:
  struct ProviderInfo {
    std::string name;

    std::unordered_map<uint64_t, StringId> string_table;
    std::unordered_map<uint64_t, fuchsia_trace_utils::ThreadInfo> thread_table;

    uint64_t ticks_per_second = 1000000000;
  };

  struct RunningThread {
    fuchsia_trace_utils::ThreadInfo info;
    int64_t start_ts;
  };

  void ParseRecord(TraceBlobView);
  void RegisterProvider(uint32_t, std::string);

  TraceProcessorContext* const context_;
  std::vector<uint8_t> leftover_bytes_;

  // Map from tid to pid. Used because in some places we do not get pid info.
  // Fuchsia tids are never reused.
  std::unordered_map<uint64_t, uint64_t> pid_table_;
  std::unordered_map<uint32_t, std::unique_ptr<ProviderInfo>> providers_;
  ProviderInfo* current_provider_;

  std::unordered_map<uint32_t, RunningThread> cpu_threads_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FUCHSIA_FUCHSIA_TRACE_TOKENIZER_H_
