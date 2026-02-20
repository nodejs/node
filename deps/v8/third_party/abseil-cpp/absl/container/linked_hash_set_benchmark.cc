// Copyright 2025 The Abseil Authors.
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

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/container/linked_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "benchmark/benchmark.h"

namespace {

void BenchmarkInsertStrings(benchmark::State& state,
                            absl::FunctionRef<std::string(int)> factory) {
  std::vector<std::string> sample;
  size_t str_bytes = 0;
  for (int i = 0; i < state.range(0); ++i) {
    sample.push_back(factory(i));
    str_bytes += sample.back().size();
  }

  // Make a batch around 1Mi bytes.
  const size_t batch_size = std::max(size_t{1}, size_t{1000000} / str_bytes);
  std::vector<absl::linked_hash_set<std::string>> sets(batch_size);

  while (state.KeepRunningBatch(batch_size)) {
    state.PauseTiming();
    for (auto& set : sets) set.clear();
    state.ResumeTiming();
    for (auto& set : sets) {
      for (const auto& str : sample) {
        benchmark::DoNotOptimize(set.insert(str));
      }
    }
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));
  state.SetBytesProcessed(state.iterations() * str_bytes);
}

constexpr absl::string_view kFormatShort = "%10d";
constexpr absl::string_view kFormatLong =
    "a longer string that exceeds the SSO %10d";

void BM_InsertShortStrings_Hit(benchmark::State& state) {
  BenchmarkInsertStrings(
      state, [](int i) { return absl::StrFormat(kFormatShort, i); });
}
BENCHMARK(BM_InsertShortStrings_Hit)->Range(1, 1 << 16);

void BM_InsertLongStrings_Hit(benchmark::State& state) {
  BenchmarkInsertStrings(state,
                         [](int i) { return absl::StrFormat(kFormatLong, i); });
}
BENCHMARK(BM_InsertLongStrings_Hit)->Range(1, 1 << 16);

void BM_InsertShortStrings_Miss(benchmark::State& state) {
  BenchmarkInsertStrings(
      state, [](int i) { return absl::StrFormat(kFormatShort, i % 20); });
}
BENCHMARK(BM_InsertShortStrings_Miss)->Range(1, 1 << 16);

void BM_InsertLongStrings_Miss(benchmark::State& state) {
  BenchmarkInsertStrings(
      state, [](int i) { return absl::StrFormat(kFormatLong, i % 20); });
}
BENCHMARK(BM_InsertLongStrings_Miss)->Range(1, 1 << 16);

}  // namespace
