// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test the case where TurboFan can statically rule out -0 from the
// Math.max type.
(function() {
  function foo(x) {
    // Arrange x such that TurboFan infers type [-inf, inf] \/ MinusZero.
    x = +x;
    x = Math.round(x);
    return Object.is(-0, Math.max(1, x))
  }

  assertFalse(foo(-0));
  assertFalse(foo(-1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(-1));
})();

// Test the case where -0 is ruled out because it's strictly less than +0.
(function() {
  function foo(x) {
    // Arrange x such that TurboFan infers type [-inf, inf] \/ MinusZero.
    x = +x;
    x = Math.round(x);
    return Object.is(-0, Math.max(0, x))
  }

  assertFalse(foo(-0));
  assertFalse(foo(-1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-0));
  assertFalse(foo(-1));
})();
