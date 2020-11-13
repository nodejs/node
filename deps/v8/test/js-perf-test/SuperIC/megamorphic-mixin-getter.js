// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DETERMINISTIC_RUNS = 10000;
new BenchmarkSuite(BENCHMARK_NAME, [1000], [
  new Benchmark(BENCHMARK_NAME, false, false, DETERMINISTIC_RUNS,
                runBenchmark)
]);

function createSubclass() {
  class A {
    get super_prop() {
        return 10;
    }
  };
  class B extends A {
    test() {
      return super.super_prop;
    }
  };
  return B;
}

const NO_CLASSES = 10;
let objects = [];
for (let i = 0; i < NO_CLASSES; ++i) {
  const klass = createSubclass();
  objects.push(new klass());
}

let ix = 0;
const EXPECTED_VALUE = 10;

function runBenchmark() {
  const r = objects[ix].test();
  if (r != EXPECTED_VALUE) {
    throw new Error("Test error");
  }
  if (++ix == objects.length) {
    ix = 0;
  }
}
