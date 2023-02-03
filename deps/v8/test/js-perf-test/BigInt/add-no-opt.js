// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('bigint-util.js');

let random_bigints = [];

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [10000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);


new BenchmarkSuite(`Add-Small`, [1000], [
  new Benchmark(`Add-Small`, true, false, 0, TestAdd,
    () => SetUpRandomBigInts(32))
]);


new BenchmarkSuite(`Add-Large`, [1000], [
  new Benchmark(`Add-Large`, true, false, 0, TestAdd,
    () => SetUpRandomBigInts(8192))
]);


new BenchmarkSuite(`Add-LargerThanSmall`, [1000], [
  new Benchmark(`Add-LargerThanSmall`, true, false, 0, TestAdd,
    () => SetUpRandomBigInts(68))
]);


new BenchmarkSuite(`Add-Random`, [1000], [
  new Benchmark(`Add-Random`, true, false, 0, TestAdd,
    SetUpTestAddRandom)
]);


function SetUpRandomBigInts(bits) {
  random_bigints = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  bits = Math.floor(bits / 4) * 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bigint = RandomBigIntWithBits(bits);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function SetUpTestAddRandom() {
  random_bigints = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function TestAdd() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS - 1; ++i) {
    sum += random_bigints[i] + random_bigints[i + 1];
  }

  return sum;
}
