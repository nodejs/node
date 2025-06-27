// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-assert-types

// Test that NumberMin properly passes the kIdentifyZeros truncation.
(function() {
  function foo(x) {
    if (Math.min(x * -2, -1) == -2) return 0;
    return 1;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(1, foo(0));
  assertOptimized(foo);
})();

// Test that NumberMin properly handles 64-bit comparisons.
(function() {
  function foo(x) {
    x = x|0;
    return Math.min(x - 1, x + 1);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(-Math.pow(2, 31) - 1, foo(-Math.pow(2, 31)));
  assertEquals(Math.pow(2, 31) - 2, foo(Math.pow(2, 31) - 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-Math.pow(2, 31) - 1, foo(-Math.pow(2, 31)));
  assertEquals(Math.pow(2, 31) - 2, foo(Math.pow(2, 31) - 1));
})();
