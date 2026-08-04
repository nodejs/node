// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TEST_ITERATIONS = 100000;

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [100000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);

new BenchmarkSuite('Equal-SmiNumber', [100000], [
  new Benchmark('Equal-SmiNumber', true, false, 0, TestEqualSmiNumber, SetUp)
]);

new BenchmarkSuite('Equal-SmiOddball', [100000], [
  new Benchmark('Equal-SmiOddball', true, false, 0, TestEqualSmiOddball, SetUp)
]);

new BenchmarkSuite('Equal-NumberOddball', [100000], [
  new Benchmark('Equal-NumberOddball', true, false, 0, TestEqualNumberOddball,
    SetUp)
]);

new BenchmarkSuite('Equal-OddballOddball', [100000], [
  new Benchmark('Equal-OddballOddball', true, false, 0, TestEqualOddballOddball,
    SetUp)
]);


let smis = [];
let numbers = [];
let oddballs = [];
function SetUp() {
  for(let i = 0; i < TEST_ITERATIONS + 1; ++i) {
    smis[i] = (i % 2 == 0) ? 42 : -42;
    numbers[i] = (i % 2 == 0) ? 42.3 : -42.3;
    oddballs[i] = (i % 2 == 0);
  }
}


function TestEqualSmiNumber() {
  let result = false;
  for(let i = 0; i < TEST_ITERATIONS; ++i) {
    result = result || (11 == numbers[i]);
  }
  return result;
}


function TestEqualSmiOddball() {
  let result = false;
  for(let i = 1; i < TEST_ITERATIONS; ++i) {
    result = result || (smis[i] == false);
  }
  return result;
}


function TestEqualNumberOddball() {
  let result = false;
  for(let i = 1; i < TEST_ITERATIONS; ++i) {
    result = result || (numbers[i] == false);
  }
  return result;
}


function TestEqualOddballOddball() {
  let result = false;
  for(let i = 0; i < TEST_ITERATIONS; ++i) {
    result = result || (oddballs[i] == oddballs[i+1]);
  }
  return result;
}
