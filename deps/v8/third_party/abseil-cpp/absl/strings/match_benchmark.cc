// Copyright 2026 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/internal/raw_logging.h"
#include "absl/strings/match.h"
#include "benchmark/benchmark.h"

namespace {

void BM_ContainsIgnoreCase_Match(benchmark::State& state) {
  const size_t haystack_size = static_cast<size_t>(state.range(0));
  const size_t needle_size = static_cast<size_t>(state.range(1));
  std::string haystack(haystack_size, 'a');
  std::string needle(needle_size, 'b');

  // Place a case-insensitive match at the very end of the haystack
  haystack.replace(haystack_size - needle_size, needle_size, needle_size, 'B');

  ABSL_RAW_CHECK(absl::StrContainsIgnoreCase(haystack, needle),
                 "BM_ContainsIgnoreCase_Match must return true");

  for (auto _ : state) {
    benchmark::DoNotOptimize(absl::StrContainsIgnoreCase(haystack, needle));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(haystack_size));
}
BENCHMARK(BM_ContainsIgnoreCase_Match)
    ->Args({4, 1})
    ->Args({64, 1})
    ->Args({64, 8})
    ->Args({1024, 16})
    ->Args({65536, 1})
    ->Args({65536, 64})
    ->Args({65536, 1024})
    ->Args({65536, 16384});

void BM_ContainsIgnoreCase_Mismatch(benchmark::State& state) {
  const size_t haystack_size = static_cast<size_t>(state.range(0));
  const size_t needle_size = static_cast<size_t>(state.range(1));
  const std::string haystack(haystack_size, 'a');
  const std::string needle(needle_size, 'b');

  ABSL_RAW_CHECK(!absl::StrContainsIgnoreCase(haystack, needle),
                 "BM_ContainsIgnoreCase_Mismatch must return false");

  for (auto _ : state) {
    benchmark::DoNotOptimize(absl::StrContainsIgnoreCase(haystack, needle));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(haystack_size));
}
BENCHMARK(BM_ContainsIgnoreCase_Mismatch)
    ->Args({4, 1})
    ->Args({64, 1})
    ->Args({64, 8})
    ->Args({1024, 16})
    ->Args({65536, 1})
    ->Args({65536, 64})
    ->Args({65536, 1024})
    ->Args({65536, 16384});

void BM_ContainsIgnoreCase_Adversarial(benchmark::State& state) {
  const size_t haystack_size = static_cast<size_t>(state.range(0));
  const std::string haystack(haystack_size, 'a');
  std::string needle(haystack_size / 2, 'a');

  // Make the needle not match the haystack due to just the last character.
  needle.push_back('b');

  ABSL_RAW_CHECK(!absl::StrContainsIgnoreCase(haystack, needle),
                 "BM_ContainsIgnoreCase_Adversarial must return false");

  for (auto _ : state) {
    benchmark::DoNotOptimize(absl::StrContainsIgnoreCase(haystack, needle));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(haystack_size));
}
BENCHMARK(BM_ContainsIgnoreCase_Adversarial)->Range(16 << 10, 128 << 10);

}  // namespace
