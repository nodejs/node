// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('bigint-util.js');

let initial_sum = 0n;
let a = 0n;
let random_bigints = [];

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [10000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);


new BenchmarkSuite('Add-Zero', [1000], [
  new Benchmark('Add-Zero', true, false, 0, TestAddZero, SetUpTestAddZero)
]);


BITS_CASES.forEach((d) => {
  new BenchmarkSuite(`Add-SameSign-${d}`, [1000], [
    new Benchmark(`Add-SameSign-${d}`, true, false, 0, TestAddSameSign,
      () => SetUpTestAddSameSign(d))
  ]);
});


BITS_CASES.forEach((d) => {
  new BenchmarkSuite(`Add-DifferentSign-${d}`, [1000], [
    new Benchmark(`Add-DifferentSign-${d}`, true, false, 0,
      TestAddDifferentSign, () => SetUpTestAddDifferentSign(d))
  ]);
});


new BenchmarkSuite('Add-Random', [1000], [
  new Benchmark('Add-Random', true, false, 0, TestAddRandom,
    SetUpTestAddRandom)
]);


function SetUpTestAddZero() {
  initial_sum = 42n;
}


function TestAddZero() {
  let sum = initial_sum;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum = 0n + sum;
  }

  return sum;
}


function SetUpTestAddSameSign(bits) {
  // Add two small random positive values to make sure the sum does not grow
  // in digits.
  initial_sum = SmallRandomBigIntWithBits(bits);
  a = SmallRandomBigIntWithBits(bits);
}


function TestAddSameSign() {
  let sum = initial_sum;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum = a + sum;
  }

  return sum;
}


function SetUpTestAddDifferentSign(bits) {
  // Add a small random negative value to a large positive one to make sure the
  // sum does not shrink in digits.
  initial_sum = MaxBigIntWithBits(bits);
  a = -SmallRandomBigIntWithBits(bits);
}


function TestAddDifferentSign() {
  let sum = initial_sum;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum = a + sum;
  }

  return sum;
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


function TestAddRandom() {
  let sum = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    sum = random_bigints[i] + sum;
  }

  return sum;
}
