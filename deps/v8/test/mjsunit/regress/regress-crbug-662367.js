// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var zero = 0;

(function ConstantFoldZeroDivZero() {
  function f() {
    return 0 / zero;
  }
  assertTrue(isNaN(f()));
  assertTrue(isNaN(f()));
  %OptimizeFunctionOnNextCall(f);
  assertTrue(isNaN(f()));
})();

(function ConstantFoldMinusZeroDivZero() {
  function f() {
    return -0 / zero;
  }
  assertTrue(isNaN(f()));
  assertTrue(isNaN(f()));
  %OptimizeFunctionOnNextCall(f);
  assertTrue(isNaN(f()));
})();

(function ConstantFoldNaNDivZero() {
  function f() {
    return NaN / 0;
  }
  assertTrue(isNaN(f()));
  assertTrue(isNaN(f()));
  %OptimizeFunctionOnNextCall(f);
  assertTrue(isNaN(f()));
})();
