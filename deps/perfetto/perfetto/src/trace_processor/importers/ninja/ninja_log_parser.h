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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_NINJA_NINJA_LOG_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_NINJA_NINJA_LOG_PARSER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "src/trace_processor/chunked_trace_reader.h"
#include "src/trace_processor/trace_parser.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// This class parses Ninja's (the build system, ninja-build.org) build logs and
// turns them into traces. A ninja log typically contains the logs of >1 ninja
// invocation. We map those as follows:
// - For each ninja invocation we create one process in the trace (from the UI
//   perspective a process is a group of tracks).
// - Within each invocation we work out the parallelism from the time stamp and
//   create one thread for each concurent stream of jobs.
// Caveat: this works only if ninja didn't recompact the logs. Once recompaction
// happens (can be forced via ninja -t recompact) there is no way to identify
// the boundaries of each build (recompaction deletes, for each hash, all but
// the most recent timestamp and rewrites the log).
class NinjaLogParser : public ChunkedTraceReader {
 public:
  explicit NinjaLogParser(TraceProcessorContext*);
  ~NinjaLogParser() override;
  NinjaLogParser(const NinjaLogParser&) = delete;
  NinjaLogParser& operator=(const NinjaLogParser&) = delete;

  // ChunkedTraceReader implementation
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) override;
  void NotifyEndOfFile() override;

 private:
  struct Job {
    Job(uint32_t b, int64_t s, int64_t e, uint64_t h, const std::string& n)
        : build_id(b), start_ms(s), end_ms(e), hash(h), names(n) {}

    uint32_t build_id;  // The synthesized PID of the build "process".
    int64_t start_ms;
    int64_t end_ms;
    uint64_t hash;  // Hash of the compiler invocation cmdline.

    // Typically the one output for the compiler invocation. In case of actions
    // generating multiple outputs this contains the join of all output names.
    std::string names;
  };

  TraceProcessorContext* const ctx_;
  bool header_parsed_ = false;
  int64_t last_end_seen_ = 0;
  uint32_t cur_build_id_ = 0;
  std::vector<Job> jobs_;
  std::vector<char> log_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_NINJA_NINJA_LOG_PARSER_H_
