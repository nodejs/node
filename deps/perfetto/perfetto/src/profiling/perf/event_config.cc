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

#include "src/profiling/perf/event_config.h"

#include <linux/perf_event.h>
#include <time.h>

#include <unwindstack/Regs.h>

#include "perfetto/base/flat_set.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/profiling/normalize.h"
#include "src/profiling/perf/regs_parsing.h"

#include "protos/perfetto/config/profiling/perf_event_config.pbzero.h"

namespace perfetto {
namespace profiling {

namespace {
constexpr uint64_t kDefaultSamplingFrequencyHz = 10;
constexpr uint32_t kDefaultDataPagesPerRingBuffer = 256;  // 1 MB: 256x 4k pages
constexpr uint32_t kDefaultReadTickPeriodMs = 100;
constexpr uint32_t kDefaultRemoteDescriptorTimeoutMs = 100;

base::Optional<std::string> Normalize(const protozero::ConstChars& src) {
  // Construct a null-terminated string that will be mutated by the normalizer.
  std::vector<char> base(src.size + 1);
  memcpy(base.data(), src.data, src.size);
  base[src.size] = '\0';

  char* new_start = base.data();
  ssize_t new_sz = NormalizeCmdLine(&new_start, base.size());
  if (new_sz < 0) {
    PERFETTO_ELOG("Failed to normalize config cmdline [%s], aborting",
                  base.data());
    return base::nullopt;
  }
  return base::make_optional<std::string>(new_start,
                                          static_cast<size_t>(new_sz));
}

// returns |base::nullopt| if any of the input cmdlines couldn't be normalized.
base::Optional<TargetFilter> ParseTargetFilter(
    const protos::pbzero::PerfEventConfig::Decoder& cfg) {
  TargetFilter filter;
  for (auto it = cfg.target_cmdline(); it; ++it) {
    base::Optional<std::string> opt = Normalize(*it);
    if (!opt.has_value())
      return base::nullopt;
    filter.cmdlines.insert(std::move(opt.value()));
  }

  for (auto it = cfg.exclude_cmdline(); it; ++it) {
    base::Optional<std::string> opt = Normalize(*it);
    if (!opt.has_value())
      return base::nullopt;
    filter.exclude_cmdlines.insert(std::move(opt.value()));
  }

  for (auto it = cfg.target_pid(); it; ++it) {
    filter.pids.insert(*it);
  }

  for (auto it = cfg.exclude_pid(); it; ++it) {
    filter.exclude_pids.insert(*it);
  }
  return base::make_optional(std::move(filter));
}

constexpr bool IsPowerOfTwo(size_t v) {
  return (v != 0 && ((v & (v - 1)) == 0));
}

// returns |base::nullopt| if the input is invalid.
base::Optional<uint32_t> ChooseActualRingBufferPages(uint32_t config_value) {
  if (!config_value) {
    static_assert(IsPowerOfTwo(kDefaultDataPagesPerRingBuffer), "");
    return base::make_optional(kDefaultDataPagesPerRingBuffer);
  }

  if (!IsPowerOfTwo(config_value)) {
    PERFETTO_ELOG("kernel buffer size must be a power of two pages");
    return base::nullopt;
  }

  return base::make_optional(config_value);
}

}  // namespace

// static
base::Optional<EventConfig> EventConfig::Create(
    const DataSourceConfig& ds_config) {
  protos::pbzero::PerfEventConfig::Decoder pb_config(
      ds_config.perf_event_config_raw());

  base::Optional<TargetFilter> filter = ParseTargetFilter(pb_config);
  if (!filter.has_value())
    return base::nullopt;

  base::Optional<uint32_t> ring_buffer_pages =
      ChooseActualRingBufferPages(pb_config.ring_buffer_pages());
  if (!ring_buffer_pages.has_value())
    return base::nullopt;

  uint32_t remote_descriptor_timeout_ms =
      pb_config.remote_descriptor_timeout_ms()
          ? pb_config.remote_descriptor_timeout_ms()
          : kDefaultRemoteDescriptorTimeoutMs;

  uint32_t read_tick_period_ms = pb_config.ring_buffer_read_period_ms()
                                     ? pb_config.ring_buffer_read_period_ms()
                                     : kDefaultReadTickPeriodMs;

  uint32_t sampling_frequency = pb_config.sampling_frequency()
                                    ? pb_config.sampling_frequency()
                                    : kDefaultSamplingFrequencyHz;

  // Take the ratio of sampling and reading frequencies, which gives the
  // upper bound on number of samples per tick (for a single per-cpu buffer).
  // Overflow not a concern for sane inputs.
  uint32_t expected_samples_per_tick =
      1 + (sampling_frequency * read_tick_period_ms) / 1000;

  // Use double the expected value as the actual guardrail (don't assume that
  // periodic read task is as exact as the kernel).
  uint32_t samples_per_tick_limit = 2 * expected_samples_per_tick;
  PERFETTO_DCHECK(samples_per_tick_limit > 0);
  PERFETTO_DLOG("Capping samples (not records) per tick to [%" PRIu32 "]",
                samples_per_tick_limit);

  return EventConfig(pb_config, sampling_frequency, ring_buffer_pages.value(),
                     read_tick_period_ms, samples_per_tick_limit,
                     remote_descriptor_timeout_ms, std::move(filter.value()));
}

EventConfig::EventConfig(const protos::pbzero::PerfEventConfig::Decoder& cfg,
                         uint32_t sampling_frequency,
                         uint32_t ring_buffer_pages,
                         uint32_t read_tick_period_ms,
                         uint32_t samples_per_tick_limit,
                         uint32_t remote_descriptor_timeout_ms,
                         TargetFilter target_filter)
    : target_all_cpus_(cfg.all_cpus()),
      ring_buffer_pages_(ring_buffer_pages),
      read_tick_period_ms_(read_tick_period_ms),
      samples_per_tick_limit_(samples_per_tick_limit),
      target_filter_(std::move(target_filter)),
      remote_descriptor_timeout_ms_(remote_descriptor_timeout_ms),
      unwind_state_clear_period_ms_(cfg.unwind_state_clear_period_ms()) {
  auto& pe = perf_event_attr_;
  pe.size = sizeof(perf_event_attr);

  pe.disabled = false;

  // Ask the kernel to sample at a given frequency.
  pe.type = PERF_TYPE_SOFTWARE;
  pe.config = PERF_COUNT_SW_CPU_CLOCK;
  pe.freq = true;
  pe.sample_freq = sampling_frequency;

  pe.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_STACK_USER |
                   PERF_SAMPLE_REGS_USER;
  // PERF_SAMPLE_TIME:
  pe.clockid = CLOCK_BOOTTIME;
  pe.use_clockid = true;
  // PERF_SAMPLE_STACK_USER:
  // Needs to be < ((u16)(~0u)), and have bottom 8 bits clear.
  pe.sample_stack_user = (1u << 15);
  // PERF_SAMPLE_REGS_USER:
  pe.sample_regs_user =
      PerfUserRegsMaskForArch(unwindstack::Regs::CurrentArch());
}

}  // namespace profiling
}  // namespace perfetto
