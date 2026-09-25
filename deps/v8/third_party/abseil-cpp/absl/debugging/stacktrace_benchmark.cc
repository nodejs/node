// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stddef.h>
#include <stdint.h>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/optimization.h"
#include "absl/cleanup/cleanup.h"
#include "absl/debugging/stacktrace.h"
#include "benchmark/benchmark.h"

static bool g_enable_fixup = false;

#if ABSL_HAVE_ATTRIBUTE_WEAK
// Override these weak symbols if possible.
bool absl::internal_stacktrace::ShouldFixUpStack() { return g_enable_fixup; }
void absl::internal_stacktrace::FixUpStack(void**, uintptr_t*, int*, size_t,
                                           size_t&) {}
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace {

static constexpr int kMaxStackDepth = 100;
static constexpr int kCacheSize = (1 << 16);
void* pcs[kMaxStackDepth];

ABSL_ATTRIBUTE_NOINLINE void func(benchmark::State& state, int x, int depth) {
  if (x <= 0) {
    // Touch a significant amount of memory so that the stack is likely to be
    // not cached in the L1 cache.
    state.PauseTiming();
    int* arr = new int[kCacheSize];
    for (int i = 0; i < kCacheSize; ++i) benchmark::DoNotOptimize(arr[i] = 100);
    delete[] arr;
    state.ResumeTiming();
    benchmark::DoNotOptimize(absl::GetStackTrace(pcs, depth, 0));
    return;
  }
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
  func(state, --x, depth);
}

template <bool EnableFixup>
void BM_GetStackTrace(benchmark::State& state) {
  const Cleanup restore_state(
      [prev = g_enable_fixup]() { g_enable_fixup = prev; });
  g_enable_fixup = EnableFixup;
  int depth = state.range(0);
  for (auto s : state) {
    func(state, depth, depth);
  }
}

#if ABSL_HAVE_ATTRIBUTE_WEAK
auto& BM_GetStackTraceWithFixup = BM_GetStackTrace<true>;
BENCHMARK(BM_GetStackTraceWithFixup)->DenseRange(10, kMaxStackDepth, 10);
#endif

auto& BM_GetStackTraceWithoutFixup = BM_GetStackTrace<false>;
BENCHMARK(BM_GetStackTraceWithoutFixup)->DenseRange(10, kMaxStackDepth, 10);
}  // namespace
ABSL_NAMESPACE_END
}  // namespace absl
