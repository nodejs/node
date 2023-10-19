// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function OptimizeAndTestOverflow() {
  function f(x, y) {
    return x - y;
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(-1n, f(0n, 1n));
  assertEquals(-7n, f(2n, 9n));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(9n, f(14n, 5n));
  assertOptimized(f);
  assertEquals(-(2n ** 63n), f(-(2n ** 62n), 2n ** 62n));
  assertOptimized(f);
  // Re-prepare the function before the first deopt to ensure type feedback is
  // not cleared by an untimely gc.
  %PrepareFunctionForOptimization(f);
  assertOptimized(f);
  // CheckedInt64Sub will trigger deopt due to overflow.
  assertEquals(-(2n ** 63n) - 1n, f(-(2n ** 62n + 1n), 2n ** 62n));
  if (%Is64Bit()) {
    assertUnoptimized(f);

    assertEquals(-1n, f(0n, 1n));
    assertEquals(-7n, f(2n, 9n));
    %OptimizeFunctionOnNextCall(f);
    assertEquals(9n, f(14n, 5n));
    assertOptimized(f);
    // Ensure there is no deopt loop.
    assertEquals(-(2n ** 63n) - 1n, f(-(2n ** 62n + 1n), 2n ** 62n));
    assertOptimized(f);
  }
})();
