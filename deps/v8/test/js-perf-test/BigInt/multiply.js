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


new BenchmarkSuite('Multiply-Zero', [1000], [
  new Benchmark('Multiply-Zero', true, false, 0, TestMultiplyZero)
]);


new BenchmarkSuite('Multiply-Small', [1000], [
  new Benchmark('Multiply-Small', true, false, 0, TestMultiplySmall)
]);


new BenchmarkSuite('Multiply-Small-Truncated', [1000], [
  new Benchmark('Multiply-Small-Truncated', true, false, 0,
    TestMultiplySmallTruncated)
]);


new BenchmarkSuite('Multiply-Random', [10000], [
  new Benchmark('Multiply-Random', true, false, 0, TestMultiplyRandom,
    SetUpTestMultiplyRandom)
]);


function TestMultiplyZero() {
  let sum = 0n;

  for (let i = 0n; i < TEST_ITERATIONS; ++i) {
    sum += 0n * i;
  }

  return sum;
}


function TestMultiplySmall() {
  let sum = 0n;

  for (let i = 0n; i < TEST_ITERATIONS; ++i) {
    sum += i * (i + 1n);
  }

  return sum;
}


function TestMultiplySmallTruncated() {
  let sum = 0n;

  for (let i = 0n; i < TEST_ITERATIONS; ++i) {
    sum += BigInt.asIntN(64, i * (i + 1n));
  }

  return sum;
}


function SetUpTestMultiplyRandom() {
  random_bigints = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function TestMultiplyRandom() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS - 1; ++i) {
    sum += random_bigints[i] * random_bigints[i + 1];
  }

  return sum;
}
