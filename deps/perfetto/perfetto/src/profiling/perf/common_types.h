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

#ifndef SRC_PROFILING_PERF_COMMON_TYPES_H_
#define SRC_PROFILING_PERF_COMMON_TYPES_H_

#include <memory>
#include <vector>

#include <linux/perf_event.h>
#include <stdint.h>

#include <unwindstack/Error.h>
#include <unwindstack/Regs.h>

#include "src/profiling/common/unwind_support.h"

namespace perfetto {
namespace profiling {

// A parsed perf sample record (PERF_RECORD_SAMPLE from the kernel buffer).
// Self-contained, used as as input to the callstack unwinding.
struct ParsedSample {
  // move-only
  ParsedSample() = default;
  ParsedSample(const ParsedSample&) = delete;
  ParsedSample& operator=(const ParsedSample&) = delete;
  ParsedSample(ParsedSample&&) noexcept = default;
  ParsedSample& operator=(ParsedSample&&) noexcept = default;

  uint16_t cpu_mode = PERF_RECORD_MISC_CPUMODE_UNKNOWN;
  uint32_t cpu = 0;
  pid_t pid = 0;
  pid_t tid = 0;
  uint64_t timestamp = 0;
  std::unique_ptr<unwindstack::Regs> regs;
  std::vector<char> stack;
};

// Entry in an unwinding queue. Either a sample that requires unwinding, or a
// tombstoned entry (valid == false).
struct UnwindEntry {
  static UnwindEntry Invalid() { return UnwindEntry{}; }

  UnwindEntry() = default;  // for initial unwinding queue entries' state

  UnwindEntry(uint64_t _data_source_id, ParsedSample _sample)
      : valid(true),
        data_source_id(_data_source_id),
        sample(std::move(_sample)) {}

  bool valid = false;
  uint64_t data_source_id = 0;
  ParsedSample sample;
};

// Fully processed sample that is ready for output.
struct CompletedSample {
  // move-only
  CompletedSample() = default;
  CompletedSample(const CompletedSample&) = delete;
  CompletedSample& operator=(const CompletedSample&) = delete;
  CompletedSample(CompletedSample&&) noexcept = default;
  CompletedSample& operator=(CompletedSample&&) noexcept = default;

  uint16_t cpu_mode = PERF_RECORD_MISC_CPUMODE_UNKNOWN;
  uint32_t cpu = 0;
  pid_t pid = 0;
  pid_t tid = 0;
  uint64_t timestamp = 0;
  std::vector<FrameData> frames;
  unwindstack::ErrorCode unwind_error = unwindstack::ERROR_NONE;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_PERF_COMMON_TYPES_H_
