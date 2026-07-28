// Copyright 2021 the V8 project authors. All rights reserved.
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


[32, 64, 128, 256].forEach((d) => {
  new BenchmarkSuite(`AsInt64-${d}`, [1000], [
    new Benchmark(`AsInt64-${d}`, true, false, 0, TestAsInt64,
      () => SetUpTestAsIntN(d))
  ]);
});


[32, 64, 128, 256].forEach((d) => {
  new BenchmarkSuite(`AsInt32-${d}`, [1000], [
    new Benchmark(`AsInt32-${d}`, true, false, 0, TestAsInt32,
      () => SetUpTestAsIntN(d))
  ]);
});


[32, 64, 128, 256].forEach((d) => {
  new BenchmarkSuite(`AsInt8-${d}`, [1000], [
    new Benchmark(`AsInt8-${d}`, true, false, 0, TestAsInt8,
      () => SetUpTestAsIntN(d))
  ]);
});


function SetUpTestAsIntN(d) {
  random_bigints = [
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d),
    RandomBigIntWithBits(d)
  ];
}


function TestAsInt64() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = BigInt.asIntN(64, random_bigints[i % 8]);
  }

  return result;
}


function TestAsInt32() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = BigInt.asIntN(32, random_bigints[i % 8]);
  }

  return result;
}


function TestAsInt8() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = BigInt.asIntN(8, random_bigints[i % 8]);
  }

  return result;
}
