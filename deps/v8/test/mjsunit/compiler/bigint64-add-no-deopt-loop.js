// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function OptimizeAndTest() {
  function f(x, y) {
    return x + y;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(1n, f(0n, 1n));
  assertEquals(5n, f(2n, 3n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(9n, f(4n, 5n));
  assertOptimized(f);
  // CheckBigInt64 should trigger deopt.
  assertEquals(-(2n ** 63n), f(-(2n ** 63n), 0n));
  if (%Is64Bit()) {
    assertUnoptimized(f);

    %PrepareFunctionForOptimization(f);
    assertEquals(1n, f(0n, 1n));
    assertEquals(5n, f(2n, 3n));
    %OptimizeFunctionOnNextCall(f);
    assertEquals(9n, f(4n, 5n));
    assertOptimized(f);
    // Ensure there is no deopt loop.
    assertEquals(-(2n ** 63n), f(-(2n ** 63n), 0n));
    assertOptimized(f);
  }
})();

(function OptimizeAndTestOverflow() {
  function f(x, y) {
    return x + y;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(1n, f(0n, 1n));
  assertEquals(5n, f(2n, 3n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(9n, f(4n, 5n));
  assertOptimized(f);
  assertEquals(-(2n ** 63n), f(-(2n ** 62n), -(2n ** 62n)));
  assertOptimized(f);
  // CheckedBigInt64Add will trigger deopt due to overflow.
  assertEquals(-(2n ** 63n) - 1n, f(-(2n ** 62n + 1n), -(2n ** 62n)));
  if (%Is64Bit()) {
    assertUnoptimized(f);

    %PrepareFunctionForOptimization(f);
    assertEquals(1n, f(0n, 1n));
    assertEquals(5n, f(2n, 3n));
    %OptimizeFunctionOnNextCall(f);
    assertEquals(9n, f(4n, 5n));
    assertOptimized(f);
    // Ensure there is no deopt loop.
    assertEquals(-(2n ** 63n) - 1n, f(-(2n ** 62n + 1n), -(2n ** 62n)));
    assertOptimized(f);
  }
})();
