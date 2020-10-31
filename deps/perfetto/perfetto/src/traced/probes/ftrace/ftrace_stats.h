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

#ifndef SRC_TRACED_PROBES_FTRACE_FTRACE_STATS_H_
#define SRC_TRACED_PROBES_FTRACE_FTRACE_STATS_H_

#include <inttypes.h>

#include <vector>

namespace perfetto {

namespace protos {
namespace pbzero {
class FtraceStats;
class FtraceCpuStats;
}  // namespace pbzero
}  // namespace protos

struct FtraceCpuStats {
  uint64_t cpu;
  uint64_t entries;
  uint64_t overrun;
  uint64_t commit_overrun;
  uint64_t bytes_read;
  double oldest_event_ts;
  double now_ts;
  uint64_t dropped_events;
  uint64_t read_events;

  void Write(protos::pbzero::FtraceCpuStats*) const;
};

struct FtraceStats {
  std::vector<FtraceCpuStats> cpu_stats;

  void Write(protos::pbzero::FtraceStats*) const;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FTRACE_STATS_H_
