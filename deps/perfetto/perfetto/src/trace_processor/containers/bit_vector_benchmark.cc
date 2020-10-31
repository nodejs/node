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

#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/containers/bit_vector_iterators.h"

namespace {

using perfetto::trace_processor::BitVector;

bool IsBenchmarkFunctionalOnly() {
  return getenv("BENCHMARK_FUNCTIONAL_TEST_ONLY") != nullptr;
}

void BitVectorArgs(benchmark::internal::Benchmark* b) {
  std::vector<int> set_percentages;
  if (IsBenchmarkFunctionalOnly()) {
    set_percentages = std::vector<int>{50};
  } else {
    set_percentages = std::vector<int>{0, 1, 5, 50, 95, 99, 100};
  }

  for (int percentage : set_percentages) {
    b->Args({64, percentage});

    if (!IsBenchmarkFunctionalOnly()) {
      b->Args({512, percentage});
      b->Args({8192, percentage});
      b->Args({123456, percentage});
      b->Args({1234567, percentage});
    }
  }
}

BitVector BvWithSizeAndSetPercentage(uint32_t size, uint32_t set_percentage) {
  static constexpr uint32_t kRandomSeed = 29;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  BitVector bv;
  for (uint32_t i = 0; i < size; ++i) {
    if (rnd_engine() % 100 < set_percentage) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }
  }
  return bv;
}

}  // namespace

static void BM_BitVectorAppendTrue(benchmark::State& state) {
  BitVector bv;
  for (auto _ : state) {
    bv.AppendTrue();
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorAppendTrue);

static void BM_BitVectorAppendFalse(benchmark::State& state) {
  BitVector bv;
  for (auto _ : state) {
    bv.AppendFalse();
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorAppendFalse);

static void BM_BitVectorSet(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  BitVector bv = BvWithSizeAndSetPercentage(size, set_percentage);

  static constexpr uint32_t kPoolSize = 1024 * 1024;
  std::vector<bool> bit_pool(kPoolSize);
  std::vector<uint32_t> row_pool(kPoolSize);
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    bit_pool[i] = rnd_engine() % 2;
    row_pool[i] = rnd_engine() % size;
  }

  uint32_t pool_idx = 0;
  for (auto _ : state) {
    bv.Set(row_pool[pool_idx]);
    pool_idx = (pool_idx + 1) % kPoolSize;
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorSet)->Apply(BitVectorArgs);

static void BM_BitVectorClear(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  BitVector bv = BvWithSizeAndSetPercentage(size, set_percentage);

  static constexpr uint32_t kPoolSize = 1024 * 1024;
  std::vector<uint32_t> row_pool(kPoolSize);
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    row_pool[i] = rnd_engine() % size;
  }

  uint32_t pool_idx = 0;
  for (auto _ : state) {
    bv.Clear(row_pool[pool_idx]);
    pool_idx = (pool_idx + 1) % kPoolSize;
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorClear)->Apply(BitVectorArgs);

static void BM_BitVectorIndexOfNthSet(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  BitVector bv = BvWithSizeAndSetPercentage(size, set_percentage);
  static constexpr uint32_t kPoolSize = 1024 * 1024;
  std::vector<uint32_t> row_pool(kPoolSize);
  uint32_t set_bit_count = bv.GetNumBitsSet();
  if (set_bit_count == 0)
    return;

  for (uint32_t i = 0; i < kPoolSize; ++i) {
    row_pool[i] = rnd_engine() % set_bit_count;
  }

  uint32_t pool_idx = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(bv.IndexOfNthSet(row_pool[pool_idx]));
    pool_idx = (pool_idx + 1) % kPoolSize;
  }
}
BENCHMARK(BM_BitVectorIndexOfNthSet)->Apply(BitVectorArgs);

static void BM_BitVectorGetNumBitsSet(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  uint32_t count = 0;
  BitVector bv;
  for (uint32_t i = 0; i < size; ++i) {
    bool value = rnd_engine() % 100 < set_percentage;
    if (value) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }

    if (value)
      count++;
  }

  uint32_t res = count;
  for (auto _ : state) {
    benchmark::DoNotOptimize(res &= bv.GetNumBitsSet());
  }
  PERFETTO_CHECK(res == count);
}
BENCHMARK(BM_BitVectorGetNumBitsSet)->Apply(BitVectorArgs);

static void BM_BitVectorResize(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  static constexpr uint32_t kPoolSize = 1024 * 1024;
  static constexpr uint32_t kMaxSize = 1234567;

  std::vector<bool> resize_fill_pool(kPoolSize);
  std::vector<uint32_t> resize_count_pool(kPoolSize);
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    resize_fill_pool[i] = rnd_engine() % 2;
    resize_count_pool[i] = rnd_engine() % kMaxSize;
  }

  uint32_t pool_idx = 0;
  BitVector bv;
  for (auto _ : state) {
    bv.Resize(resize_count_pool[pool_idx], resize_fill_pool[pool_idx]);
    pool_idx = (pool_idx + 1) % kPoolSize;
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorResize);

static void BM_BitVectorRangeFixedSize(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  std::vector<uint32_t> resize_fill_pool(size);
  for (uint32_t i = 0; i < size; ++i) {
    resize_fill_pool[i] = rnd_engine() % 100 < set_percentage ? 90 : 100;
  }

  for (auto _ : state) {
    auto filler = [&resize_fill_pool](uint32_t i) PERFETTO_ALWAYS_INLINE {
      return resize_fill_pool[i] < 95;
    };
    BitVector bv = BitVector::Range(0, size, filler);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorRangeFixedSize)->Apply(BitVectorArgs);

static void BM_BitVectorUpdateSetBits(benchmark::State& state) {
  static constexpr uint32_t kRandomSeed = 42;
  std::minstd_rand0 rnd_engine(kRandomSeed);

  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  BitVector bv;
  BitVector picker;
  for (uint32_t i = 0; i < size; ++i) {
    bool value = rnd_engine() % 100 < set_percentage;
    if (value) {
      bv.AppendTrue();

      bool picker_value = rnd_engine() % 2;
      if (picker_value) {
        picker.AppendTrue();
      } else {
        picker.AppendFalse();
      }
    } else {
      bv.AppendFalse();
    }
  }

  for (auto _ : state) {
    state.PauseTiming();
    BitVector copy = bv.Copy();
    state.ResumeTiming();

    copy.UpdateSetBits(picker);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitVectorUpdateSetBits)->Apply(BitVectorArgs);

static void BM_BitVectorSetBitsIterator(benchmark::State& state) {
  uint32_t size = static_cast<uint32_t>(state.range(0));
  uint32_t set_percentage = static_cast<uint32_t>(state.range(1));

  BitVector bv = BvWithSizeAndSetPercentage(size, set_percentage);
  for (auto _ : state) {
    for (auto it = bv.IterateSetBits(); it; it.Next()) {
      benchmark::DoNotOptimize(it.index());
    }
  }
}
BENCHMARK(BM_BitVectorSetBitsIterator)->Apply(BitVectorArgs);
