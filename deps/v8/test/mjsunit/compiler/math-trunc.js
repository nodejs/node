// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Ensure that the typing rule for Math.trunc deals correctly with
// inputs in the range (-1.0,0.0), which are mapped to -0.
(function() {
  function foo(x) {
    // Arrange x such that TurboFan infers type PlainNumber \/ NaN.
    x = +x;
    x = Math.abs(x) - 1.0;
    return Object.is(-0, Math.trunc(x));
  }

  assertFalse(foo(1.5));
  assertTrue(foo(0.5));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(1.5));
  assertTrue(foo(0.5));
})();

// Ensure that the typing rule for Math.trunc deals correctly with
// NaN inputs, which are mapped to NaN.
(function() {
  function foo(x) {
    // Arrange x such that TurboFan infers type PlainNumber \/ NaN.
    x = +x;
    x = Math.abs(x) - 1.0;
    return Object.is(NaN, Math.trunc(x));
  }

  assertFalse(foo(1.5));
  assertTrue(foo(NaN));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(1.5));
  assertTrue(foo(NaN));
})();
