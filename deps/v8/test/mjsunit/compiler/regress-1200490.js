// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function() {
  function foo(a) {
    var y = 1;
    var z = 0;
    if (a) {
      y = -1;
      z = -0;
    }
    return z + (0 * y);
  }
  assertEquals(-0, foo(true));
  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-0, foo(true));
})();


(function() {
  function foo(a) {
    var x = a ? -0 : 0;
    return x + (x % 1);
  }
  assertEquals(-0, foo(true));
  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-0, foo(true));
})();


(function() {
  function foo(a) {
    var x = a ? -0 : 0;
    return x + (x % -1);
  }
  assertEquals(-0, foo(true));
  %PrepareFunctionForOptimization(foo);
  assertEquals(0, foo(false));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-0, foo(true));
})();
