// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "src/base/macros.h"

static void BM_Empty(benchmark::State& state) {
  for (auto _ : state) {
    USE(_);
  }
}

// Register the function as a benchmark. The empty benchmark ensures that the
// framework compiles and links as expected.
BENCHMARK(BM_Empty);
