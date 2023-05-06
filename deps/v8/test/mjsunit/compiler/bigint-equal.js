// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

const bi = 42n;

function Equal(x, y) {
  return x == y;
}

function StrictEqual(x, y) {
  return x === y;
}

function Test(f, large) {
  assertEquals(false, f(1n, 2n));
  assertEquals(false, f(1n, -1n));
  assertEquals(true, f(-1n, -1n));
  assertEquals(true, f(bi, bi));
  assertEquals(false, f(2n ** 63n - 1n, -(2n ** 63n) + 1n));
  if (large) {
    assertEquals(false, f(2n ** 63n, -(2n ** 63n)));
    assertEquals(true, f(13n ** 70n, 13n ** 70n));
  }
}

function OptAndTest(f, large) {
  %PrepareFunctionForOptimization(f);
  Test(f, large);
  assertUnoptimized(f);
  %OptimizeFunctionOnNextCall(f);
  Test(f, large);
  assertOptimized(f);
}

OptAndTest(Equal, false);
OptAndTest(StrictEqual, false);
if (%Is64Bit()) {
  // Should deopt on large bigints and there should not be deopt loops.
  OptAndTest(Equal, true);
  OptAndTest(StrictEqual, true);
}
