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

#include "hwy/contrib/thread_pool/spin.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>

#include "hwy/aligned_allocator.h"          // HWY_ALIGNMENT
#include "hwy/contrib/thread_pool/futex.h"  // NanoSleep
#include "hwy/contrib/thread_pool/thread_pool.h"
#include "hwy/contrib/thread_pool/topology.h"
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"
#include "hwy/timer.h"

namespace hwy {
namespace {

struct TestPingPongT {
  template <class Spin>
  void operator()(const Spin& spin) const {
    constexpr size_t kU32PerLine = HWY_ALIGNMENT / 4;
    constexpr size_t kF64PerLine = HWY_ALIGNMENT / 8;
    alignas(HWY_ALIGNMENT) std::atomic<uint32_t> thread_active[kU32PerLine];
    alignas(HWY_ALIGNMENT) std::atomic<uint32_t> thread_done[kU32PerLine];

    thread_active[0].store(0, std::memory_order_release);
    thread_done[0].store(0, std::memory_order_release);
    hwy::ThreadPool pool(1);
    HWY_ASSERT(pool.NumWorkers() == 2);

    const double t0 = hwy::platform::Now();
    std::atomic_flag error = ATOMIC_FLAG_INIT;

    alignas(HWY_ALIGNMENT) std::atomic<size_t> reps1;
    alignas(HWY_ALIGNMENT) std::atomic<size_t> reps2;

    alignas(HWY_ALIGNMENT) std::atomic<double> before_thread_done[kF64PerLine];
    alignas(HWY_ALIGNMENT) std::atomic<double> before_thread_go[kF64PerLine];
    alignas(HWY_ALIGNMENT) std::atomic<double> ack_thread_done[kF64PerLine];
    alignas(HWY_ALIGNMENT) std::atomic<double> ack_thread_release[kF64PerLine];

    const auto kAcq = std::memory_order_acquire;
    const auto kRel = std::memory_order_release;
    pool.Run(0, 2, [&](uint64_t task, size_t thread) {
      HWY_ASSERT(task == thread);
      if (task == 0) {  // new thread
        SpinResult result = spin.UntilDifferent(0, thread_active[0]);
        ack_thread_release[0].store(hwy::platform::Now(), kRel);
        reps1.store(result.reps);
        if (!NanoSleep(20 * 1000 * 1000)) {
          error.test_and_set();
        }
        before_thread_done[0].store(hwy::platform::Now(), kRel);
        thread_done[0].store(1, kRel);
      } else {  // main thread
        if (!NanoSleep(30 * 1000 * 1000)) {
          error.test_and_set();
        }
        // Release the thread.
        before_thread_go[0].store(hwy::platform::Now(), kRel);
        thread_active[0].store(1, kRel);
        // Wait for it to finish.
        const size_t reps = spin.UntilEqual(1, thread_done[0]);
        ack_thread_done[0].store(hwy::platform::Now(), kRel);
        reps2.store(reps);
      }
    });

    const double t1 = hwy::platform::Now();
    const double elapsed = t1 - t0;
    const double latency1 =
        ack_thread_release[0].load(kAcq) - before_thread_go[0].load(kAcq);
    const double latency2 =
        ack_thread_done[0].load(kAcq) - before_thread_done[0].load(kAcq);
    fprintf(stderr,
            "Elapsed time: %f us; reps1=%zu, reps2=%zu, latency=%f %f us\n",
            elapsed * 1E6, reps1.load(), reps2.load(), latency1 * 1E6,
            latency2 * 1E6);
    // Unless NanoSleep failed to sleep, this should take 50ms+epsilon.
    HWY_ASSERT(error.test_and_set() || elapsed > 25E-3);
  }
};  // namespace hwy

// Simple mutex.
TEST(SpinTest, TestPingPong) {
  if (!HaveThreadingSupport()) {
    HWY_WARN("Threads not supported, skipping test\n");
    return;
  }

  const SpinType spin_type = DetectSpin();
  fprintf(stderr, "Spin method : %s\n", ToString(spin_type));
  CallWithSpin(spin_type, TestPingPongT());
}

}  // namespace
}  // namespace hwy

HWY_TEST_MAIN();
