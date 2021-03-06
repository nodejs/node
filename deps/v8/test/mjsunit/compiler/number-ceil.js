// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test that NumberCeil propagates kIdentifyZeros truncations.
(function() {
  function foo(x) {
    return Math.abs(Math.ceil(x * -2));
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(1));
  assertEquals(4, foo(2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(1));
  assertEquals(4, foo(2));
  assertOptimized(foo);
  // Now `foo` should stay optimized even if `x * -2` would produce `-0`.
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();
