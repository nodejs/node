// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function LessThan(x, y) {
  return x < y;
}

function LessThanOrEqual(x, y) {
  return x <= y;
}

function GreaterThan(x, y) {
  return x > y;
}

function GreaterThanOrEqual(x, y) {
  return x >= y;
}

function Test(f, large, lt, eq) {
  assertEquals(lt, f(1n, 2n));
  assertEquals(!lt, f(0n, -1n));
  assertEquals(eq, f(-42n, -42n));
  assertEquals(!lt, f(-(2n ** 62n), -(2n ** 63n) + 1n));
  assertEquals(lt, f(-(2n ** 63n) + 1n, (2n ** 63n) - 1n));
  if (large) {
    assertEquals(lt, f(2n ** 63n - 1n, 2n ** 63n));
    assertEquals(!lt, f(-(2n ** 63n) + 1n, -(2n ** 63n)));
    assertEquals(lt, f(-(13n ** 70n), 13n ** 70n));     // Different signs
    assertEquals(!lt, f(13n ** 70n, -(13n ** 70n)));
    assertEquals(lt, f(13n ** 80n, 13n ** 90n));        // Different lengths
    assertEquals(!lt, f(-(13n ** 70n), -(13n ** 80n))); // Same length
    assertEquals(eq, f(13n ** 70n, 13n ** 70n));
  }
}

function OptAndTest(f, large) {
  const lt = f === LessThan || f === LessThanOrEqual;
  const eq = f === LessThanOrEqual || f === GreaterThanOrEqual;
  %PrepareFunctionForOptimization(f);
  Test(f, large, lt, eq);
  assertUnoptimized(f);
  %OptimizeFunctionOnNextCall(f);
  Test(f, large, lt, eq);
  assertOptimized(f);
}

OptAndTest(LessThan, false);
OptAndTest(LessThanOrEqual, false);
OptAndTest(GreaterThan, false);
OptAndTest(GreaterThanOrEqual, false);
if (%Is64Bit()) {
  // Should deopt on large bigints and there should not be deopt loops.
  OptAndTest(LessThan, true);
  OptAndTest(LessThanOrEqual, true);
  OptAndTest(GreaterThan, true);
  OptAndTest(GreaterThanOrEqual, true);
}
