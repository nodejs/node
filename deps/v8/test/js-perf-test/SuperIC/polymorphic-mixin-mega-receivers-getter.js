// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DETERMINISTIC_RUNS = 10000;
new BenchmarkSuite(BENCHMARK_NAME, [1000], [
  new Benchmark(BENCHMARK_NAME, false, false, DETERMINISTIC_RUNS, runBenchmark)
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

const b1 = createSubclass();
const b2 = createSubclass();

class C0 extends b1 { };
class C1 extends b2 { };
class C2 extends b1 { };
class C3 extends b2 { };
class C4 extends b1 { };
class C5 extends b2 { };
class C6 extends b1 { };
class C7 extends b2 { };
class C8 extends b1 { };
class C9 extends b2 { };

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
