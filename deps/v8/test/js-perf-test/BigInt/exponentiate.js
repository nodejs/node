// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('bigint-util.js');

let random_exponents = [];

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [10000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);


new BenchmarkSuite('Exponentiate-Base-Two', [10000], [
  new Benchmark('Exponentiate-Base-Two', true, false, 0,
    TestExponentiateBaseTwo, SetUpTestExponentiateBaseTwo)
]);


function SetUpTestExponentiateBaseTwo() {
  random_exponents = [];
  // Restrict the maximum length of exponents to 20 bits so that the durations
  // are reasonable and BigIntTooBig exceptions can be avoided.
  const max_in_4bits = 20 / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    // Exponents are non-negative.
    random_exponents.push(bigint);
  }
}


function TestExponentiateBaseTwo() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum += 2n ** random_exponents[i];
  }

  return sum;
}
