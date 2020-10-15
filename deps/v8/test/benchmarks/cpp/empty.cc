// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

static void BM_Empty(benchmark::State& state) {
  for (auto _ : state) {
  }
}
// Register the function as a benchmark
BENCHMARK(BM_Empty);
