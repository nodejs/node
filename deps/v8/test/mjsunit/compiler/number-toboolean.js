// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test that NumberToBoolean properly passes the kIdentifyZeros truncation
// for Signed32 \/ MinusZero inputs.
(function() {
  function foo(x) {
    if (x * -2) return 1;
    return 0;
  }

  assertEquals(1, foo(1));
  assertEquals(1, foo(2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1));
  assertEquals(1, foo(2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();


// Test that NumberToBoolean properly passes the kIdentifyZeros truncation
// for Unsigned32 \/ MinusZero inputs.
(function() {
  function foo(x) {
    x = x | 0;
    if (Math.max(x * -2, 0)) return 1;
    return 0;
  }

  assertEquals(1, foo(-1));
  assertEquals(1, foo(-2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(-1));
  assertEquals(1, foo(-2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();
