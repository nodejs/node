// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>

#include "hwy/base.h"           // Abort
#include "hwy/cache_control.h"  // Pause
#include "hwy/contrib/thread_pool/thread_pool.h"
#include "hwy/profiler.h"
#include "hwy/timer.h"

namespace hwy {
namespace {

HWY_INLINE void Spin(const double min_time) {
  const double t0 = hwy::platform::Now();
  for (;;) {
    hwy::Pause();
    const double elapsed = hwy::platform::Now() - t0;
    if (elapsed > min_time) {
      break;
    }
  }
}

HWY_NOINLINE void Spin10us() {
  PROFILER_FUNC;
  Spin(10E-6);
}

HWY_NOINLINE void Spin20us() {
  PROFILER_FUNC;
  Spin(20E-6);
}

HWY_NOINLINE void CallTwoSpin() {
  PROFILER_ZONE("NearZeroBecauseOfChildZones");
  {
    PROFILER_ZONE("spin30");
    Spin(30E-6);
  }
  {
    PROFILER_ZONE("spin60");
    Spin(60E-6);
  }
}

HWY_NOINLINE void Compute(Profiler& p, HWY_MAYBE_UNUSED size_t thread) {
  static const auto zone = p.AddZone("Compute");
  PROFILER_ZONE3(p, static_cast<uint8_t>(thread), zone);
  for (int rep = 0; rep < 100; ++rep) {
    double total = 0.0;
    for (int i = 0; i < 200 - rep; ++i) {
      total += std::pow(0.9, i);
    }
    if (std::abs(total - 10.0) > 1E-2) {
      HWY_ABORT("unexpected total %f", total);
    }
  }
}

HWY_NOINLINE void TestThreads(Profiler& p) {
  PROFILER_ZONE("NearZeroBecauseOfThreadedChildZones");
  {
    PROFILER_ZONE("Create pool1");
    ThreadPool pool(3);
    pool.Run(0, 5, [&p](uint64_t /*task*/, HWY_MAYBE_UNUSED size_t thread) {
      Compute(p, thread);
    });
  }

  {
    PROFILER_ZONE("Create pool2");
    ThreadPool pool(8);
    pool.Run(0, 8, [&p](uint64_t /*task*/, HWY_MAYBE_UNUSED size_t thread) {
      Compute(p, thread);
    });
  }
}

HWY_NOINLINE void CallTestThreadPlus20us(Profiler& p) {
  PROFILER_FUNC;
  TestThreads(p);
  Spin(20E-6);
}

HWY_NOINLINE void CallCallTestThreadPlus10us(Profiler& p) {
  PROFILER_FUNC;
  CallTestThreadPlus20us(p);
  Spin(10E-6);
}

void ProfilerExample(Profiler& p) {
  PROFILER_ZONE("NearZeroDespiteInclusiveChildZone");
  {
    static const auto zone =
        p.AddZone("Inclusive 10+20", ProfilerFlags::kInclusive);
    PROFILER_ZONE3(p, 0, zone);
    Spin20us();
    Spin10us();
  }
  CallTwoSpin();
  CallCallTestThreadPlus10us(p);
}

}  // namespace
}  // namespace hwy

int main(int /*argc*/, char* /*argv*/[]) {
  hwy::ProfilerExample(hwy::Profiler::Get());
  PROFILER_PRINT_RESULTS();  // after all zones have exited
  return 0;
}
