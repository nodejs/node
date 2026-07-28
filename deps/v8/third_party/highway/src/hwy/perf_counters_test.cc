// Copyright 2024 Google LLC
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

#include "hwy/perf_counters.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "hwy/contrib/thread_pool/futex.h"  // NanoSleep
#include "hwy/nanobenchmark.h"  // Unpredictable1
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"
#include "hwy/timer.h"

namespace hwy {
namespace {

using ::hwy::platform::PerfCounters;

void ReadAndPrint(uint64_t r, double* values) {
  char cpu100[100];
  const bool have_stop = hwy::platform::HaveTimerStop(cpu100);
  const uint64_t t0 = timer::Start();

  PerfCounters counters;
  const uint64_t t1 = have_stop ? timer::Stop() : timer::Start();
  const double elapsed_ns =
      static_cast<double>(t1 - t0) * 1E9 / platform::InvariantTicksPerSecond();
  fprintf(stderr, "r: %d, any valid %d extrapolate %f, overhead %.1f ns\n",
          static_cast<int>(r), counters.AnyValid(), counters.MaxExtrapolate(),
          elapsed_ns);

  if (counters.AnyValid()) {
    HWY_ASSERT(counters.MaxExtrapolate() >= 1.0);
  }

  counters.Foreach([&counters, values](double val, PerfCounters::Counter c) {
    HWY_ASSERT(counters.IsValid(c));
    fprintf(stderr, "%-20s: %.3E\n", PerfCounters::Name(c), val);
    values[static_cast<size_t>(c)] = val;
  });
  PerfCounters::StopAllAndReset();
}

// Ensures a memory-intensive workload has high memory-related counters.
TEST(PerfCountersTest, TestMem) {
  RandomState rng;
  if (!PerfCounters::Init() || !PerfCounters::StartAll()) {
    HWY_WARN("Perf counters unavailable, skipping test\n");
    return;
  }
  // Force L3 cache misses (loads).
  std::vector<uint64_t> big_array(128 * 1024 * 1024);
  for (uint64_t& x : big_array) {
    x = rng() & static_cast<uint64_t>(hwy::Unpredictable1());
  }
  const uint64_t r = big_array[rng() & 0xFFFF];

  double values[64] = {0.0};
  ReadAndPrint(r, values);

  // Note that counters might not be available, and values differ considerably
  // for debug/sanitizer builds.
  HWY_ASSERT(values[PerfCounters::kRefCycles] == 0.0 ||
             values[PerfCounters::kRefCycles] > 1E8);  // 470M..9B
  HWY_ASSERT(values[PerfCounters::kInstructions] == 0.0 ||
             values[PerfCounters::kInstructions] > 1E5);  // 1.5M..10B
  HWY_ASSERT(values[PerfCounters::kPageFaults] == 0.0 ||
             values[PerfCounters::kPageFaults] > 1);  // 4..500K
  HWY_ASSERT(values[PerfCounters::kBranches] == 0.0 ||
             values[PerfCounters::kBranches] > 1E5);           // > 900K
  HWY_ASSERT(values[PerfCounters::kBranchMispredicts] < 1E9);  // 273K..400M

  HWY_ASSERT(values[PerfCounters::kL3Loads] == 0.0 ||
             values[PerfCounters::kL3Loads] > 10.0);  // ~90K, 50 with L4
  HWY_ASSERT(values[PerfCounters::kL3Stores] == 0.0 ||
             values[PerfCounters::kL3Stores] > 10.0);  // 9K..5M

  HWY_ASSERT(values[PerfCounters::kCacheRefs] == 0.0 ||
             values[PerfCounters::kCacheRefs] > 1E4);  // 75K..66M
  HWY_ASSERT(values[PerfCounters::kCacheMisses] == 0.0 ||
             values[PerfCounters::kCacheMisses] > 1.0);  // 10..51M
  HWY_ASSERT(values[PerfCounters::kBusCycles] == 0.0 ||
             values[PerfCounters::kBusCycles] > 1E6);  // 8M
}

// Ensures a branch-heavy workload has high branch-related counters and not
// too high memory-related counters.
TEST(PerfCountersTest, RunBranches) {
  RandomState rng;
  if (!PerfCounters::Init() || !PerfCounters::StartAll()) {
    HWY_WARN("Perf counters unavailable, skipping test\n");
    return;
  }

  // Branch-heavy, non-constexpr calculation so we see changes to counters.
  const size_t iters =
      static_cast<size_t>(hwy::Unpredictable1()) * 100000 + (rng() & 1);
  uint64_t r = rng();
  for (size_t i = 0; i < iters; ++i) {
    if (PopCount(rng()) < 36) {
      r += rng() & 0xFF;
    } else {
      // Entirely different operation to ensure there is a branch.
      r >>= 1;
    }
    // Ensure test runs long enough for counter multiplexing to happen.
    NanoSleep(100 * 1000);
  }

  double values[64] = {0.0};
  ReadAndPrint(r, values);

  // Note that counters might not be available, and values differ considerably
  // for debug/sanitizer builds.
  HWY_ASSERT(values[PerfCounters::kRefCycles] == 0.0 ||
             values[PerfCounters::kRefCycles] > 1E3);  // 13K..18M
  HWY_ASSERT(values[PerfCounters::kInstructions] == 0.0 ||
             values[PerfCounters::kInstructions] > 100.0);  // 900..2M
  HWY_ASSERT(values[PerfCounters::kBranches] == 0.0 ||
             values[PerfCounters::kBranches] > 100.0);  // 1K..273K
  HWY_ASSERT(values[PerfCounters::kBranchMispredicts] == 0 ||
             values[PerfCounters::kBranchMispredicts] > 10.0);  // 65..5K

  HWY_ASSERT(values[PerfCounters::kL3Loads] < 1E8);       // 174K..12M
  HWY_ASSERT(values[PerfCounters::kL3Stores] < 1E7);      // 44K..1.8M
  HWY_ASSERT(values[PerfCounters::kCacheRefs] < 1E9);     // 5M..104M
  HWY_ASSERT(values[PerfCounters::kCacheMisses] < 1E8);   // 500K..10M
  HWY_ASSERT(values[PerfCounters::kBusCycles] < 1E11);    // 1M..10B
  HWY_ASSERT(values[PerfCounters::kPageFaults] < 1E4);    // 0..1.1K (in SDE)
}

}  // namespace
}  // namespace hwy

HWY_TEST_MAIN();
