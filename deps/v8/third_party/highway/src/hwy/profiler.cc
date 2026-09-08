// Copyright 2025 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/profiler.h"

#include "hwy/highway_export.h"  // HWY_DLLEXPORT

#if PROFILER_ENABLED

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hwy/base.h"
#include "hwy/robust_statistics.h"
#include "hwy/timer.h"

#endif  // PROFILER_ENABLED

namespace hwy {

#if PROFILER_ENABLED

static constexpr bool kPrintOverhead = true;

// Must zero-init because `ThreadFunc` calls `SetGlobalIdx()` potentially after
// this is first used in the `pool::Worker` ctor.
/*static*/ thread_local size_t Profiler::s_global_idx = 0;

// Detects duration of a zero-length zone: timer plus packet overhead.
static uint64_t DetectSelfOverhead(Profiler& profiler, size_t global_idx) {
  static const profiler::ZoneHandle zone = profiler.AddZone("DetectSelf");
  profiler::Results results;
  const size_t kNumSamples = 25;
  uint32_t samples[kNumSamples];
  for (size_t idx_sample = 0; idx_sample < kNumSamples; ++idx_sample) {
    // Enough for stable measurements, but only about 50 ms startup cost.
    const size_t kNumDurations = 700;
    uint32_t durations[kNumDurations];
    for (size_t idx_duration = 0; idx_duration < kNumDurations;
         ++idx_duration) {
      {
        PROFILER_ZONE3(profiler, global_idx, zone);
      }
      durations[idx_duration] =
          static_cast<uint32_t>(profiler.GetFirstDurationAndReset(global_idx));
    }
    samples[idx_sample] = robust_statistics::Mode(durations, kNumDurations);
  }
  return robust_statistics::Mode(samples, kNumSamples);
}

// Detects average duration of a zero-length zone, after deducting self
// overhead. This accounts for the delay before/after capturing start/end
// timestamps, for example due to fence instructions in timer::Start/Stop.
static uint64_t DetectChildOverhead(Profiler& profiler, size_t global_idx,
                                    uint64_t self_overhead) {
  static const profiler::ZoneHandle zone = profiler.AddZone("DetectChild");
  // Enough for stable measurements, but only about 50 ms startup cost.
  const size_t kMaxSamples = 30;
  uint32_t samples[kMaxSamples];
  size_t num_samples = 0;
  // Upper bound because timer resolution might be too coarse to get nonzero.
  for (size_t s = 0; s < 2 * kMaxSamples && num_samples < kMaxSamples; ++s) {
    const size_t kNumDurations = 50;
    uint32_t durations[kNumDurations];
    for (size_t d = 0; d < kNumDurations; ++d) {
      constexpr size_t kReps = 500;
      HWY_FENCE;
      const uint64_t t0 = timer::Start();
      for (size_t r = 0; r < kReps; ++r) {
        PROFILER_ZONE3(profiler, global_idx, zone);
      }
      const uint64_t t1 = timer::Stop();
      HWY_FENCE;
      // We are measuring the total, not individual zone durations, to include
      // cross-zone overhead.
      (void)profiler.GetFirstDurationAndReset(global_idx);

      const uint64_t avg_duration = (t1 - t0 + kReps / 2) / kReps;
      durations[d] = static_cast<uint32_t>(
          profiler::PerWorker::ClampedSubtract(avg_duration, self_overhead));
    }
    samples[num_samples] = robust_statistics::Mode(durations, kNumDurations);
    // Overhead is nonzero, but we often measure zero; skip them to prevent
    // getting a zero result.
    num_samples += (samples[num_samples] != 0);
  }
  return num_samples == 0 ? 0 : robust_statistics::Mode(samples, num_samples);
}

Profiler::Profiler() {
  const uint64_t t0 = timer::Start();

  char cpu[100];
  if (HWY_UNLIKELY(!platform::HaveTimerStop(cpu))) {
    HWY_ABORT("CPU %s is too old for PROFILER_ENABLED=1, exiting", cpu);
  }

  // `ThreadPool` calls `Profiler::Get()` before it creates threads, hence this
  // is guaranteed to be running on the main thread.
  constexpr size_t kMain = 0;
  // Must be called before any use of `PROFILER_ZONE*/PROFILER_FUNC*`. This runs
  // only once because `Profiler` is a singleton.
  ReserveWorker(kMain);
  SetGlobalIdx(kMain);

  profiler::Overheads overheads;
  // WARNING: must pass in `*this` and use `PROFILER_ZONE3` to avoid calling
  // `Profiler::Get()`, because that would re-enter the magic static init.
  overheads.self = DetectSelfOverhead(*this, kMain);
  overheads.child = DetectChildOverhead(*this, kMain, overheads.self);
  for (size_t worker = 0; worker < profiler::kMaxWorkers; ++worker) {
    workers_[worker].SetOverheads(overheads);
  }

  HWY_IF_CONSTEXPR(kPrintOverhead) {
    printf("Self overhead: %.0f; child: %.0f; elapsed %.1f ms\n",
           static_cast<double>(overheads.self),
           static_cast<double>(overheads.child),
           static_cast<double>(timer::Stop() - t0) /
               platform::InvariantTicksPerSecond() * 1E3);
  }
}

#endif  // PROFILER_ENABLED

// Even if disabled, we want to export the symbol.
HWY_DLLEXPORT Profiler& Profiler::Get() {
  static Profiler* profiler = new Profiler();
  return *profiler;
}

}  // namespace hwy
