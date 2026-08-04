// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('bigint-util.js');

let a = 0n;
let random_bigints = [];
let small_shift_values = [];

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [100], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);

new BenchmarkSuite('ShiftLeft-Small', [1000], [
  new Benchmark('ShiftLeft-Small', true, false, 0,
    TestShiftLeftSmall, SetUpTestShiftLeftSmall)
]);

new BenchmarkSuite('ShiftLeft-Random', [1000], [
  new Benchmark('ShiftLeft-Random', true, false, 0,
    TestShiftLeftRandom, SetUpTestShiftLeftRandom)
]);

new BenchmarkSuite('ShiftLeft-Growing', [1000], [
  new Benchmark('ShiftLeft-Growing', true, false, 0,
    TestShiftLeftGrowing, SetUpTestShiftLeftGrowing)
]);

new BenchmarkSuite('ShiftRight-Maximum', [1000], [
  new Benchmark('ShiftRight-Maximum', true, false, 0,
    TestShiftRightMaximum, SetUpTestShiftRightMaximum)
]);

new BenchmarkSuite('ShiftRight-ToZero', [1000], [
  new Benchmark('ShiftRight-ToZero', true, false, 0,
    TestShiftRightToZero, SetUpTestShiftRightToZero)
]);

new BenchmarkSuite('ShiftRight-Small', [1000], [
  new Benchmark('ShiftRight-Small', true, false, 0,
    TestShiftRightSmall, SetUpTestShiftRightSmall)
]);

new BenchmarkSuite('ShiftRight-Random', [1000], [
  new Benchmark('ShiftRight-Random', true, false, 0,
    TestShiftRightRandom, SetUpTestShiftRightRandom)
]);

new BenchmarkSuite('ShiftRight-Shrinking', [1000], [
  new Benchmark('ShiftRight-Shrinking', true, false, 0,
    TestShiftRightShrinking, SetUpTestShiftRightShrinking)
]);


function SetUpRandomBigInts() {
  random_bigints = [];
  // RandomBigIntWithBits needs multiples of 4 bits.
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < Math.max(TEST_ITERATIONS, SLOW_TEST_ITERATIONS); ++i) {
    const bits = Math.floor(Math.random() * max_in_4bits) * 4;
    const bigint = RandomBigIntWithBits(bits);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
  }
}


function SetUpTestShiftLeftSmall() {
  random_bigints = [];
  // Set up all values such that the left shifted values still fit into one
  // digit.
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    random_bigints[i] = SmallRandomBigIntWithBits(16);
    small_shift_values[i] = SmallRandomBigIntWithBits(4);
  }
}

function TestShiftLeftSmall() {
  let result = 0n;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = random_bigints[i] << small_shift_values[i];
  }
  return result;
}


function SetUpTestShiftLeftRandom() {
  if (RANDOM_BIGINTS_MAX_BITS + 2**16 > BIGINT_MAX_BITS) {
    throw "Invalid Test Configuration";
  }
  small_shift_values = [];
  SetUpRandomBigInts();
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    small_shift_values[i] = SmallRandomBigIntWithBits(16);
  }
}

function TestShiftLeftRandom() {
  let result = 0n;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = random_bigints[i] << small_shift_values[i];
  }
  return result;
}


function SetUpTestShiftLeftGrowing() {
  if (2**4 + (1000 * (2**8)) > BIGINT_MAX_BITS) {
    throw "Invalid Test Configuration";
  }
  small_shift_values = [];
  for (let i = 0; i < 1000; ++i) {
    small_shift_values[i] = SmallRandomBigIntWithBits(8);
  }
}

function TestShiftLeftGrowing() {
  let result = a;
  for (let i = 0; i < 1000; ++i) {
    result = result << small_shift_values[i];
  }
  return result;
}


function SetUpTestShiftRightMaximum() {
  // Right shifting by 2^80 will shift out all bits.
  a = SmallRandomBigIntWithBits(80);
  SetUpRandomBigInts();
}

function TestShiftRightMaximum() {
  let result = 0n;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = random_bigints[i] >> a;
  }
  return result;
}


function SetUpTestShiftRightToZero() {
  // Choose a such that every random BigInt >> a will result in 0n/-1n.
  a = BigInt(2 * RANDOM_BIGINTS_MAX_BITS);
  SetUpRandomBigInts();
}

function TestShiftRightToZero() {
  let result = 0n;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = random_bigints[i] >> a;
  }
  return result;
}


function SetUpTestShiftRightSmall() {
  random_bigints = [];
  // We construct small BigInts with 32 bits such that they have a single digit
  // even on 32bit architectures.
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    random_bigints[i] = SmallRandomBigIntWithBits(32);
    small_shift_values[i] = BigInt(Math.floor(Math.random() * 30) + 1);
  }
}

function TestShiftRightSmall() {
  let result = 0n;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = random_bigints[i] >> small_shift_values[i];
  }
  return result;
}


function SetUpTestShiftRightRandom() {
  random_bigints = [];
  small_shift_values = [];
  const max_in_4bits = RANDOM_BIGINTS_MAX_BITS / 4;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    const bits_in_4 = Math.floor(Math.random() * max_in_4bits);
    const shift_in_4 = Math.floor(Math.random() * bits_in_4);
    const bigint = RandomBigIntWithBits(bits_in_4 * 4);
    random_bigints.push(Math.random() < 0.5 ? -bigint : bigint);
    small_shift_values.push(BigInt(shift_in_4 * 4));
  }
}

function TestShiftRightRandom() {
  let result = 0n;
  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = random_bigints[i] >> small_shift_values[i];
  }
  return result;
}


function SetUpTestShiftRightShrinking() {
  small_shift_values = [];
  // Choose values such that shifting right by 1000 times will not make the
  // result to go 0n/-1n.
  a = RandomBigIntWithBits(16*1000);
  for (let i = 0; i < 1000; ++i) {
    small_shift_values[i] = SmallRandomBigIntWithBits(4);
  }
}

function TestShiftRightShrinking() {
  let result = a;
  for (let i = 0; i < 1000; ++i) {
    result = result >> small_shift_values[i];
  }
  return result;
}
