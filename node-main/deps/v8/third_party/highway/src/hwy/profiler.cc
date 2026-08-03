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

constexpr bool kPrintOverhead = true;

// Initialize to an invalid value to detect when `InitThread` was not called.
// `Profiler()` does so for the main thread and `ThreadPool()` for all workers.
/*static*/ thread_local size_t Profiler::s_thread = ~size_t{0};
/*static*/ std::atomic<size_t> Profiler::s_num_threads{0};

// Detects duration of a zero-length zone: timer plus packet overhead.
static uint64_t DetectSelfOverhead(Profiler& profiler, size_t thread) {
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
        static const profiler::ZoneHandle zone =
            profiler.AddZone("DetectSelfOverhead");
        PROFILER_ZONE3(profiler, /*thread=*/0, zone);
      }
      durations[idx_duration] = static_cast<uint32_t>(
          profiler.GetThread(thread).GetFirstDurationAndReset(
              thread, profiler.Accumulators()));
    }
    samples[idx_sample] = robust_statistics::Mode(durations, kNumDurations);
  }
  return robust_statistics::Mode(samples, kNumSamples);
}

// Detects average duration of a zero-length zone, after deducting self
// overhead. This accounts for the delay before/after capturing start/end
// timestamps, for example due to fence instructions in timer::Start/Stop.
static uint64_t DetectChildOverhead(Profiler& profiler, size_t thread,
                                    uint64_t self_overhead) {
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
        static const profiler::ZoneHandle zone =
            profiler.AddZone("DetectChildOverhead");
        PROFILER_ZONE3(profiler, /*thread=*/0, zone);
      }
      const uint64_t t1 = timer::Stop();
      HWY_FENCE;
      // We are measuring the total, not individual zone durations, to include
      // cross-zone overhead.
      (void)profiler.GetThread(thread).GetFirstDurationAndReset(
          thread, profiler.Accumulators());

      const uint64_t avg_duration = (t1 - t0 + kReps / 2) / kReps;
      durations[d] = static_cast<uint32_t>(
          profiler::PerThread::ClampedSubtract(avg_duration, self_overhead));
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

  InitThread();

  char cpu[100];
  if (HWY_UNLIKELY(!platform::HaveTimerStop(cpu))) {
    HWY_ABORT("CPU %s is too old for PROFILER_ENABLED=1, exiting", cpu);
  }

  profiler::Overheads overheads;
  // WARNING: must pass in Profiler& and use `PROFILER_ZONE3` to avoid calling
  // `Profiler::Get()` here, because that would re-enter the magic static init.
  constexpr size_t kThread = 0;
  overheads.self = DetectSelfOverhead(*this, kThread);
  overheads.child = DetectChildOverhead(*this, kThread, overheads.self);
  for (size_t thread = 0; thread < profiler::kMaxThreads; ++thread) {
    threads_[thread].SetOverheads(overheads);
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
  static Profiler profiler;
  return profiler;
}

}  // namespace hwy
