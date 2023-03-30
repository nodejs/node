// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function TestBitwiseAnd(a, b) {
  return a & b;
}

function OptimizeAndTest(fn) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(0b1000n, fn(0b1100n, 0b1010n));
  assertEquals(-0b1000n, fn(-0b100n, -0b110n));
  assertEquals(0b1000n, fn(-0b100n, 0b1010n));
  assertEquals(0b1000n, fn(0b1100n, -0b110n));
  // The result grows out of one digit
  assertEquals(-(2n ** 64n), fn(-(2n ** 63n + 1n), -(2n ** 63n)));

  %OptimizeFunctionOnNextCall(fn);
  fn(0b1100n, 0b1010n);
  assertOptimized(fn);

  assertEquals(0b1000n, fn(0b1100n, 0b1010n));
  assertEquals(-0b1000n, fn(-0b100n, -0b110n));
  assertEquals(0b1000n, fn(-0b100n, 0b1010n));
  assertEquals(0b1000n, fn(0b1100n, -0b110n));
  // The result grows out of one digit
  assertEquals(-(2n ** 64n), fn(-(2n ** 63n + 1n), -(2n ** 63n)));
  assertOptimized(fn);

  assertEquals(0b1000, fn(0b1100, 0b1010));
  assertUnoptimized(fn);
}

OptimizeAndTest(TestBitwiseAnd);
