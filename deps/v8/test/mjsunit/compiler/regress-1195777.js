// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function() {
  function foo(b) {
    let y = (new Date(42)).getMilliseconds();
    let x = -1;
    if (b) x = 0xFFFF_FFFF;
    return y < Math.max(1 << y, x, 1 + y);
  }
  assertTrue(foo(true));
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(true));
})();


(function() {
  function foo(b) {
    let x = 0;
    if (b) x = -1;
    return x == Math.max(-1, x >>> Infinity);
  }
  assertFalse(foo(true));
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(true));
})();


(function() {
  function foo(b) {
    let x = -1;
    if (b) x = 0xFFFF_FFFF;
    return -1 < Math.max(0, x, -1);
  }
  assertTrue(foo(true));
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(true));
})();


(function() {
  function foo(b) {
    let x = 0x7FFF_FFFF;
    if (b) x = 0;
    return 0 < (Math.max(-5 >>> x, -5) % -5);
  }
  assertTrue(foo(true));
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(true));
})();
