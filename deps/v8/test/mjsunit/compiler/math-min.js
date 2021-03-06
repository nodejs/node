// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test the case where TurboFan can statically rule out -0 from the
// Math.min type.
(function() {
  function foo(x) {
    // Arrange x such that TurboFan infers type [-inf, inf] \/ MinusZero.
    x = +x;
    x = Math.round(x);
    return Object.is(-0, Math.min(-1, x))
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(-0));
  assertFalse(foo(-1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(-1));
})();

// Test the case where +0 is ruled out because it's strictly greater than -0.
(function() {
  function foo(x) {
    // Arrange x such that TurboFan infers type [-inf, inf] \/ MinusZero.
    x = +x;
    x = Math.round(x);
    return Object.is(+0, Math.min(-0, x))
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(-0));
  assertFalse(foo(-1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(-1));
})();
