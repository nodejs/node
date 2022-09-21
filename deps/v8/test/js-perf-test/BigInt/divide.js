// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('bigint-util.js');

let random_dividends = []
let random_divisors = [];

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [10000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);


new BenchmarkSuite('Divide-One', [1000], [
  new Benchmark('Divide-One', true, false, 0, TestDivideOne)
]);


new BenchmarkSuite('Divide-Small', [1000], [
  new Benchmark('Divide-Small', true, false, 0, TestDivideSmall,
    SetUpTestDivideSmall)
]);


new BenchmarkSuite('Divide-Small-Truncated', [1000], [
  new Benchmark('Divide-Small-Truncated', true, false, 0, TestDivideSmallTruncated)
]);


new BenchmarkSuite('Divide-Random', [10000], [
  new Benchmark('Divide-Random', true, false, 0, TestDivideRandom,
    SetUpTestDivideRandom)
]);


function TestDivideOne() {
  let sum = 0n;

  for (let i = 0n; i < TEST_ITERATIONS; ++i) {
    sum += i / 1n;
  }

  return sum;
}


function SetUpTestDivideSmall() {
  random_dividends = [];
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bigint = RandomBigIntWithBits(64);
    random_dividends.push(Math.random() < 0.5 ? -bigint : bigint);
  }

  random_divisors = [];
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bigint = RandomBigIntWithBits(32);
    random_divisors.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function TestDivideSmall() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum += random_dividends[i] / random_divisors[i];
  }

  return sum;
}


function TestDivideSmallTruncated() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum += BigInt.asIntN(64, random_dividends[i] / random_divisors[i]);
  }

  return sum;
}


function SetUpTestDivideRandom() {
  random_dividends = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_dividends.push(Math.random() < 0.5 ? -bigint : bigint);
  }

  random_divisors = [];
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    // Avoid divide-by-zero
    const bits = Math.floor(1 + Math.random() * (max_in_4bits - 1)) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_divisors.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function TestDivideRandom() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum += random_dividends[i] / random_divisors[i];
  }

  return sum;
}
