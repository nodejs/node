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

#include <benchmark/benchmark.h>

#include "src/trace_processor/containers/sparse_vector.h"

namespace {

static constexpr uint32_t kPoolSize = 100000;
static constexpr uint32_t kSize = 123456;

}  // namespace

static void BM_SparseVectorAppendNonNull(benchmark::State& state) {
  std::vector<uint8_t> data_pool(kPoolSize);

  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    data_pool[i] = rnd_engine() % std::numeric_limits<uint8_t>::max();
  }

  perfetto::trace_processor::SparseVector<uint8_t> sv;
  uint32_t pool_idx = 0;
  for (auto _ : state) {
    sv.Append(data_pool[pool_idx]);
    pool_idx = (pool_idx + 1) % kPoolSize;
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SparseVectorAppendNonNull);

static void BM_SparseVectorGetNonNull(benchmark::State& state) {
  std::vector<uint32_t> idx_pool(kPoolSize);

  perfetto::trace_processor::SparseVector<uint8_t> sv;
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);
  for (uint32_t i = 0; i < kSize; ++i) {
    sv.Append(rnd_engine() % std::numeric_limits<uint8_t>::max());
  }
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    idx_pool[i] = rnd_engine() % kSize;
  }

  uint32_t pool_idx = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(sv.Get(idx_pool[pool_idx]));
    pool_idx = (pool_idx + 1) % kPoolSize;
  }
}
BENCHMARK(BM_SparseVectorGetNonNull);
