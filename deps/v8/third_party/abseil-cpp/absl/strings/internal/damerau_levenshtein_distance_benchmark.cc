// Copyright 2022 The Abseil Authors.
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

#include <string>

#include "absl/strings/internal/damerau_levenshtein_distance.h"
#include "benchmark/benchmark.h"

namespace {

std::string MakeTestString(int desired_length, int num_edits) {
  std::string test(desired_length, 'x');
  for (int i = 0; (i < num_edits) && (i * 8 < desired_length); ++i) {
    test[i * 8] = 'y';
  }
  return test;
}

void BenchmarkArgs(benchmark::internal::Benchmark* benchmark) {
  // Each column is 8 bytes.
  const auto string_size = {1, 8, 64, 100};
  const auto num_edits = {1, 2, 16, 16, 64, 80};
  const auto distance_cap = {1, 2, 3, 16, 64, 80};
  for (const int s : string_size) {
    for (const int n : num_edits) {
      for (const int d : distance_cap) {
        if (n > s) continue;
        benchmark->Args({s, n, d});
      }
    }
  }
}

using absl::strings_internal::CappedDamerauLevenshteinDistance;
void BM_Distance(benchmark::State& state) {
  std::string s1 = MakeTestString(state.range(0), 0);
  std::string s2 = MakeTestString(state.range(0), state.range(1));
  const size_t cap = state.range(2);
  for (auto _ : state) {
    CappedDamerauLevenshteinDistance(s1, s2, cap);
  }
}
BENCHMARK(BM_Distance)->Apply(BenchmarkArgs);

}  // namespace
