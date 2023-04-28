// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('bigint-util.js');

let initial_diff = 0n;
let a = 0n;
let random_bigints = [];

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [10000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);


new BenchmarkSuite('Subtract-Zero', [1000], [
  new Benchmark('Subtract-Zero', true, false, 0, TestSubtractZero,
    SetUpTestSubtractZero)
]);


BITS_CASES.forEach((d) => {
  new BenchmarkSuite(`Subtract-SameSign-${d}`, [1000], [
    new Benchmark(`Subtract-SameSign-${d}`, true, false, 0,
      TestSubtractSameSign, () => SetUpTestSubtractSameSign(d))
  ]);
});


BITS_CASES.forEach((d) => {
  new BenchmarkSuite(`Subtract-DifferentSign-${d}`, [1000], [
    new Benchmark(`Subtract-DifferentSign-${d}`, true, false, 0,
      TestSubtractDifferentSign, () => SetUpTestSubtractDifferentSign(d))
  ]);
});


new BenchmarkSuite('Subtract-Random', [1000], [
  new Benchmark('Subtract-Random', true, false, 0, TestSubtractRandom,
    SetUpTestSubtractRandom)
]);


function SetUpTestSubtractSameSign(bits) {
  // Subtract a small random negative value from the maximal negative number to
  // make sure the result stays negative.
  initial_diff = -MaxBigIntWithBits(bits);
  a = -SmallRandomBigIntWithBits(bits);
}


function TestSubtractSameSign() {
  let diff = initial_diff;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    diff = diff - a;
  }

  return diff;
}


function SetUpTestSubtractDifferentSign(bits) {
  // Subtract a small random negative value from a small positive value so that
  // the differnce stays positive but does not grow in digits.
  initial_diff = SmallRandomBigIntWithBits(bits);
  a = -SmallRandomBigIntWithBits(bits);
}


function TestSubtractDifferentSign() {
  let diff = initial_diff;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    diff = diff - a;
  }

  return diff;
}


function SetUpTestSubtractRandom() {
  random_bigints = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function TestSubtractRandom() {
  let diff = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    diff = diff - random_bigints[i];
  }

  return diff;
}


function SetUpTestSubtractZero() {
  initial_diff = 42n;
}


function TestSubtractZero() {
  let diff = initial_diff;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    diff = diff - 0n;
  }

  return diff;
}
