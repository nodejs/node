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


new BenchmarkSuite('BitwiseAnd-Zero', [1000], [
  new Benchmark('BitwiseAnd-Zero', true, false, 0, TestBitwiseAndZero)
]);


new BenchmarkSuite('BitwiseAnd-Small', [1000], [
  new Benchmark('BitwiseAnd-Small', true, false, 0, TestBitwiseAnd,
    SetUpTestBitwiseAndSmall)
]);


new BenchmarkSuite('BitwiseAnd-Small-Truncated', [1000], [
  new Benchmark('BitwiseAnd-Small-Truncated', true, false, 0,
    TestBitwiseAndTruncated, SetUpTestBitwiseAndSmall)
]);


new BenchmarkSuite('BitwiseAnd-Random', [1000], [
  new Benchmark('BitwiseAnd-Random', true, false, 0, TestBitwiseAnd,
    SetUpTestBitwiseAndRandom)
]);


function TestBitwiseAndZero() {
  let result = 0n;

  for (let i = 0n; i < TEST_ITERATIONS; ++i) {
    result += 0n & i;
  }

  return result;
}


function SetUpTestBitwiseAndSmall() {
  random_bigints = [];
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bigint = RandomBigIntWithBits(64);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function SetUpTestBitwiseAndRandom() {
  random_bigints = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function TestBitwiseAnd() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS - 1; ++i) {
    result += random_bigints[i] & random_bigints[i + 1];
  }

  return result;
}


function TestBitwiseAndTruncated() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS - 1; ++i) {
    // Truncated explicitly
    result += BigInt.asIntN(64, random_bigints[i] & random_bigints[i + 1]);
  }

  return result;
}
