// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

load('bigint-util.js');

// This dummy ensures that the feedback for benchmark.run() in the Measure
// function from base.js is not monomorphic, thereby preventing the benchmarks
// below from being inlined. This ensures consistent behavior and comparable
// results.
new BenchmarkSuite('Prevent-Inline-Dummy', [10000], [
  new Benchmark('Prevent-Inline-Dummy', true, false, 0, () => {})
]);


[32, 64, 128, 256].forEach((d) => {
  new BenchmarkSuite(`AsUint64-${d}`, [1000], [
    new Benchmark(`AsUint64-${d}`, true, false, 0, TestAsUint64,
      () => SetUpTestAsUintN(d))
  ]);
});


[32, 64, 128, 256].forEach((d) => {
  new BenchmarkSuite(`AsUint32-${d}`, [1000], [
    new Benchmark(`AsUint32-${d}`, true, false, 0, TestAsUint32,
      () => SetUpTestAsUintN(d))
  ]);
});


[32, 64, 128, 256].forEach((d) => {
  new BenchmarkSuite(`AsUint8-${d}`, [1000], [
    new Benchmark(`AsUint8-${d}`, true, false, 0, TestAsUint8,
      () => SetUpTestAsUintN(d))
  ]);
});


function SetUpTestAsUintN(d) {
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


function TestAsUint64() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = BigInt.asUintN(64, random_bigints[i % 8]);
  }

  return result;
}


function TestAsUint32() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = BigInt.asUintN(32, random_bigints[i % 8]);
  }

  return result;
}


function TestAsUint8() {
  let result = 0n;

  for (let i = 0; i < TEST_ITERATIONS; ++i) {
    result = BigInt.asUintN(8, random_bigints[i % 8]);
  }

  return result;
}
