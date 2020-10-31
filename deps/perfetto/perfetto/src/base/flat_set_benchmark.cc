// Copyright (C) 2019 The Android Open Source Project
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

#include <random>
#include <set>
#include <unordered_set>

#include <benchmark/benchmark.h>

#include "perfetto/base/flat_set.h"

namespace {

std::vector<int> GetRandData(int num_pids) {
  std::vector<int> rnd_data;
  std::minstd_rand0 rng(0);
  static constexpr int kDistinctValues = 100;
  for (int i = 0; i < num_pids; i++)
    rnd_data.push_back(static_cast<int>(rng()) % kDistinctValues);
  return rnd_data;
}

bool IsBenchmarkFunctionalOnly() {
  return getenv("BENCHMARK_FUNCTIONAL_TEST_ONLY") != nullptr;
}

void BenchmarkArgs(benchmark::internal::Benchmark* b) {
  if (IsBenchmarkFunctionalOnly()) {
    b->Arg(64);
  } else {
    b->RangeMultiplier(2)->Range(64, 4096);
  }
}

}  // namespace

template <typename SetType>
static void BM_SetInsert(benchmark::State& state) {
  std::vector<int> rnd_data = GetRandData(state.range(0));
  for (auto _ : state) {
    SetType iset;
    for (const int val : rnd_data)
      iset.insert(val);
    benchmark::DoNotOptimize(iset);
    benchmark::DoNotOptimize(iset.begin());
    benchmark::ClobberMemory();
  }
}

using perfetto::base::FlatSet;
BENCHMARK_TEMPLATE(BM_SetInsert, FlatSet<int>)->Apply(BenchmarkArgs);
BENCHMARK_TEMPLATE(BM_SetInsert, std::set<int>)->Apply(BenchmarkArgs);
BENCHMARK_TEMPLATE(BM_SetInsert, std::unordered_set<int>)->Apply(BenchmarkArgs);
