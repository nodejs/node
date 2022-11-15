// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function OptimizeAndTestDivZero() {
  function f(x, y) {
    return x % y;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(0n, f(0n, 1n));
  assertEquals(-5n, f(-32n, 9n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4n, f(14n, 5n));
  assertOptimized(f);
  // Re-prepare the function before the first deopt to ensure type feedback is
  // not cleared by an untimely gc.
  %PrepareFunctionForOptimization(f);
  assertOptimized(f);
  // CheckedInt64Mod will trigger deopt due to divide-by-zero.
  assertThrows(() => f(42n, 0n), RangeError);
  if (%Is64Bit()) {
    assertUnoptimized(f);

    assertEquals(0n, f(0n, 1n));
    assertEquals(-5n, f(-32n, 9n));
    %OptimizeFunctionOnNextCall(f);
    assertEquals(4n, f(14n, 5n));
    assertOptimized(f);
    // Ensure there is no deopt loop.
    assertThrows(() => f(42n, 0n), RangeError);
    assertOptimized(f);
  }
})();

(function OptimizeAndTestOverflow() {
  function f(x, y) {
    return x % y;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(0n, f(0n, 1n));
  assertEquals(-5n, f(-32n, 9n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4n, f(14n, 5n));
  assertOptimized(f);
  // Re-prepare the function before the first deopt to ensure type feedback is
  // not cleared by an umtimely gc.
  %PrepareFunctionForOptimization(f);
  assertOptimized(f);
  // CheckedInt64Mod will trigger deopt due to overflow.
  assertEquals(0n, f(-(2n ** 63n), -1n));
  if (%Is64Bit()) {
    assertUnoptimized(f);

    assertEquals(0n, f(0n, 1n));
    assertEquals(-5n, f(-32n, 9n));
    %OptimizeFunctionOnNextCall(f);
    assertEquals(4n, f(14n, 5n));
    assertOptimized(f);
    // Ensure there is no deopt loop.
    assertEquals(0n, f(-(2n ** 63n), -1n));
    assertOptimized(f);
  }
})();
