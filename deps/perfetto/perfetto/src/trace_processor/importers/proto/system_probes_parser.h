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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_SYSTEM_PROBES_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_SYSTEM_PROBES_PARSER_H_

#include <vector>

#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class SystemProbesParser {
 public:
  using ConstBytes = protozero::ConstBytes;

  explicit SystemProbesParser(TraceProcessorContext*);

  void ParseProcessTree(ConstBytes);
  void ParseProcessStats(int64_t timestamp, ConstBytes);
  void ParseSysStats(int64_t ts, ConstBytes);
  void ParseSystemInfo(ConstBytes);
  void ParseCpuInfo(ConstBytes);

 private:
  void ParseThreadStats(int64_t timestamp, uint32_t pid, ConstBytes);

  TraceProcessorContext* const context_;

  const StringId utid_name_id_;
  const StringId num_forks_name_id_;
  const StringId num_irq_total_name_id_;
  const StringId num_softirq_total_name_id_;
  const StringId num_irq_name_id_;
  const StringId num_softirq_name_id_;
  const StringId cpu_times_user_ns_id_;
  const StringId cpu_times_user_nice_ns_id_;
  const StringId cpu_times_system_mode_ns_id_;
  const StringId cpu_times_idle_ns_id_;
  const StringId cpu_times_io_wait_ns_id_;
  const StringId cpu_times_irq_ns_id_;
  const StringId cpu_times_softirq_ns_id_;
  const StringId oom_score_adj_id_;
  std::vector<StringId> meminfo_strs_id_;
  std::vector<StringId> vmstat_strs_id_;

  // Maps a proto field number for memcounters in ProcessStats::Process to
  // their StringId. Keep kProcStatsProcessSize equal to 1 + max proto field
  // id of ProcessStats::Process.
  static constexpr size_t kProcStatsProcessSize = 11;
  std::array<StringId, kProcStatsProcessSize> proc_stats_process_names_{};

  uint64_t ms_per_tick_ = 0;

  // Maps CPU frequency indices to CPU strings: time_in_state.cpuN.
  std::vector<StringId> thread_time_in_state_cpu_str_ids_;

  // Maps CPU frequency indices to frequency strings.
  std::vector<StringId> thread_time_in_state_cpu_freq_ids_;
};
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_SYSTEM_PROBES_PARSER_H_
