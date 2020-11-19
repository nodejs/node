// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DETERMINISTIC_RUNS = 10000;
new BenchmarkSuite(BENCHMARK_NAME, [1000], [
  new Benchmark(BENCHMARK_NAME, false, false, DETERMINISTIC_RUNS,
                runBenchmark)
]);

class A { };
A.prototype.super_prop = 10;

class B extends A {
  test() {
    return super.super_prop;
  }
};

class C0 extends B { };
class C1 extends B { };
class C2 extends B { };
class C3 extends B { };
class C4 extends B { };
class C5 extends B { };
class C6 extends B { };
class C7 extends B { };
class C8 extends B { };
class C9 extends B { };

const objects = [
  new C0(),
  new C1(),
  new C2(),
  new C3(),
  new C4(),
  new C5(),
  new C6(),
  new C7(),
  new C8(),
  new C9(),
];

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
