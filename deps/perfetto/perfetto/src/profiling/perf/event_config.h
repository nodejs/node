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

#ifndef SRC_PROFILING_PERF_EVENT_CONFIG_H_
#define SRC_PROFILING_PERF_EVENT_CONFIG_H_

#include <string>

#include <linux/perf_event.h>
#include <stdint.h>
#include <sys/types.h>

#include "perfetto/base/flat_set.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/tracing/core/data_source_config.h"

#include "protos/perfetto/config/profiling/perf_event_config.pbzero.h"

namespace perfetto {
namespace profiling {

// Parsed whitelist/blacklist for filtering samples.
// An empty whitelist set means that all targets are allowed.
struct TargetFilter {
  base::FlatSet<std::string> cmdlines;
  base::FlatSet<std::string> exclude_cmdlines;
  base::FlatSet<pid_t> pids;
  base::FlatSet<pid_t> exclude_pids;
};

// Describes a single profiling configuration. Bridges the gap between the data
// source config proto, and the raw "perf_event_attr" structs to pass to the
// perf_event_open syscall.
class EventConfig {
 public:
  static base::Optional<EventConfig> Create(const DataSourceConfig& ds_config);

  uint32_t target_all_cpus() const { return target_all_cpus_; }
  uint32_t ring_buffer_pages() const { return ring_buffer_pages_; }
  uint32_t read_tick_period_ms() const { return read_tick_period_ms_; }
  uint32_t samples_per_tick_limit() const { return samples_per_tick_limit_; }
  uint32_t remote_descriptor_timeout_ms() const {
    return remote_descriptor_timeout_ms_;
  }
  uint32_t unwind_state_clear_period_ms() const {
    return unwind_state_clear_period_ms_;
  }

  const TargetFilter& filter() const { return target_filter_; }

  perf_event_attr* perf_attr() const {
    return const_cast<perf_event_attr*>(&perf_event_attr_);
  }

 private:
  EventConfig(const protos::pbzero::PerfEventConfig::Decoder& cfg,
              uint32_t sampling_frequency,
              uint32_t ring_buffer_pages,
              uint32_t read_tick_period_ms,
              uint32_t samples_per_tick_limit,
              uint32_t remote_descriptor_timeout_ms,
              TargetFilter target_filter);

  // If true, process all system-wide samples.
  const bool target_all_cpus_;

  // Size (in 4k pages) of each per-cpu ring buffer shared with the kernel.
  // Must be a power of two.
  const uint32_t ring_buffer_pages_;

  // Parameter struct for |perf_event_open| calls.
  struct perf_event_attr perf_event_attr_ = {};

  // How often the ring buffers should be read.
  const uint32_t read_tick_period_ms_;

  // Guardrail for the amount of samples a given read attempt will extract from
  // *each* per-cpu buffer.
  const uint32_t samples_per_tick_limit_;

  // Parsed whitelist/blacklist for filtering samples.
  const TargetFilter target_filter_;

  // Timeout for proc-fd lookup.
  const uint32_t remote_descriptor_timeout_ms_;

  // Optional period for clearing cached unwinder state. Skipped if zero.
  const uint32_t unwind_state_clear_period_ms_;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_PERF_EVENT_CONFIG_H_
