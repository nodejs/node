// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function OptimizeAndTest() {
  function ShiftRight(a, b) {
    return a >> b;
  }
  %PrepareFunctionForOptimization(ShiftRight);
  assertEquals(0n, ShiftRight(0n, 42n));
  assertEquals(-42n, ShiftRight(-42n, 0n));
  assertEquals(-3n, ShiftRight(-5n, 1n));
  assertEquals(0n, ShiftRight(42n, 2n ** 64n));
  assertEquals(-1n, ShiftRight(-42n, 64n));
  assertEquals(-1n, ShiftRight(-42n, 2n ** 64n));

  %OptimizeFunctionOnNextCall(ShiftRight);
  assertEquals(0n, ShiftRight(0n, 42n));
  assertEquals(-42n, ShiftRight(-42n, 0n));
  assertEquals(-3n, ShiftRight(-5n, 1n));
  assertEquals(0n, ShiftRight(42n, 2n ** 64n));
  assertEquals(-1n, ShiftRight(-42n, 64n));
  assertEquals(-1n, ShiftRight(-42n, 2n ** 64n));
  assertOptimized(ShiftRight);

  assertThrows(() => ShiftRight(1n, -(2n ** 30n)), RangeError);
  assertUnoptimized(ShiftRight);
})();

(function OptimizeAndTest() {
  function ShiftRightUnsignedByPositive(a) {
    return BigInt.asIntN(62, BigInt.asUintN(64, a) >> 42n);
  }
  %PrepareFunctionForOptimization(ShiftRightUnsignedByPositive);
  assertEquals(0n, ShiftRightUnsignedByPositive(0n));

  %OptimizeFunctionOnNextCall(ShiftRightUnsignedByPositive);
  assertEquals(0n, ShiftRightUnsignedByPositive(42n));
  assertEquals(4194303n, ShiftRightUnsignedByPositive(-42n));
  assertEquals(2n ** 20n, ShiftRightUnsignedByPositive(1n + 2n ** 62n));
  assertEquals(3145727n,
               ShiftRightUnsignedByPositive(-1n - 2n ** 62n - 2n ** 64n));
  assertOptimized(ShiftRightUnsignedByPositive);

  assertThrows(() => ShiftRightUnsignedByPositive(0), TypeError);
  if (%Is64Bit()) {
    // BigInt truncation is not inlined on 32-bit platforms so there is no
    // checks for BigInt, thus deopt will not be triggered.
    assertUnoptimized(ShiftRightUnsignedByPositive);
  }
})();

(function OptimizeAndTest() {
  function ShiftRightSignedByPositive(a) {
    return BigInt.asIntN(62, BigInt.asIntN(64, a) >> 42n);
  }
  %PrepareFunctionForOptimization(ShiftRightSignedByPositive);
  assertEquals(0n, ShiftRightSignedByPositive(0n));

  %OptimizeFunctionOnNextCall(ShiftRightSignedByPositive);
  assertEquals(0n, ShiftRightSignedByPositive(42n));
  assertEquals(-1n, ShiftRightSignedByPositive(-42n));
  assertEquals(2n ** 20n, ShiftRightSignedByPositive(1n + 2n ** 62n));
  assertEquals(-(2n ** 20n),
               ShiftRightSignedByPositive(-(2n ** 62n) - 2n ** 64n));
  assertOptimized(ShiftRightSignedByPositive);

  assertThrows(() => ShiftRightSignedByPositive(0), TypeError);
  if (%Is64Bit()) {
    // BigInt truncation is not inlined on 32-bit platforms so there is no
    // checks for BigInt, thus deopt will not be triggered.
    assertUnoptimized(ShiftRightSignedByPositive);
  }
})();

(function OptimizeAndTest() {
  const minus42 = -42n;
  function ShiftRightByNegative(a) {
    return BigInt.asIntN(62, a >> minus42);
  }
  %PrepareFunctionForOptimization(ShiftRightByNegative);
  assertEquals(0n, ShiftRightByNegative(0n));

  %OptimizeFunctionOnNextCall(ShiftRightByNegative);
  assertEquals(0n, ShiftRightByNegative(0n));
  assertEquals(2n ** 42n, ShiftRightByNegative(1n));
  assertEquals(2n ** 42n, ShiftRightByNegative(1n + 2n ** 62n));
  assertEquals(-(2n ** 42n), ShiftRightByNegative(-1n - 2n ** 64n));
  assertOptimized(ShiftRightByNegative);

  assertThrows(() => ShiftRightByNegative(0), TypeError);
  assertUnoptimized(ShiftRightByNegative);
})();

(function OptimizeAndTest() {
  function ShiftRightBy64(a) {
    return BigInt.asIntN(62, BigInt.asUintN(64, a) >> 64n);
  }
  %PrepareFunctionForOptimization(ShiftRightBy64);
  assertEquals(0n, ShiftRightBy64(0n));

  %OptimizeFunctionOnNextCall(ShiftRightBy64);
  assertEquals(0n, ShiftRightBy64(0n));
  assertEquals(0n, ShiftRightBy64(1n));
  assertEquals(0n, ShiftRightBy64(1n + 2n ** 62n));
  assertEquals(0n, ShiftRightBy64(-1n - 2n ** 64n));
  assertOptimized(ShiftRightBy64);

  assertThrows(() => ShiftRightBy64(0), TypeError);
  if (%Is64Bit()) {
    // BigInt truncation is not inlined on 32-bit platforms so there is no
    // checks for BigInt, thus deopt will not be triggered.
    assertUnoptimized(ShiftRightBy64);
  }
})();

(function OptimizeAndTest() {
  const bi = 2n ** 64n;
  function ShiftRightByLarge(a) {
    return BigInt.asIntN(62, BigInt.asIntN(64, a) >> bi);
  }
  %PrepareFunctionForOptimization(ShiftRightByLarge);
  assertEquals(0n, ShiftRightByLarge(0n));

  %OptimizeFunctionOnNextCall(ShiftRightByLarge);
  assertEquals(0n, ShiftRightByLarge(0n));
  assertEquals(-1n, ShiftRightByLarge(-1n));
  assertEquals(0n, ShiftRightByLarge(1n + 2n ** 62n));
  assertEquals(-1n, ShiftRightByLarge(-1n - 2n ** 64n));
  assertOptimized(ShiftRightByLarge);

  assertThrows(() => ShiftRightByLarge(0), TypeError);
  if (%Is64Bit()) {
    // BigInt truncation is not inlined on 32-bit platforms so there is no
    // checks for BigInt, thus deopt will not be triggered.
    assertUnoptimized(ShiftRightByLarge);
  }
})();
