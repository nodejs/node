// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo(x) {
    x = x | 0;
    return Number.parseInt(x + 1);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(0));
  assertEquals(2, foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(Math.pow(2, 31), foo(Math.pow(2, 31) - 1));
})();

(function() {
  function foo(x) {
    x = x | 0;
    return Number.parseInt(x + 1, 0);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(0));
  assertEquals(2, foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(Math.pow(2, 31), foo(Math.pow(2, 31) - 1));
})();

(function() {
  function foo(x) {
    x = x | 0;
    return Number.parseInt(x + 1, 10);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(0));
  assertEquals(2, foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(Math.pow(2, 31), foo(Math.pow(2, 31) - 1));
})();

(function() {
  function foo(x) {
    x = x | 0;
    return Number.parseInt(x + 1, undefined);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo(0));
  assertEquals(2, foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(Math.pow(2, 31), foo(Math.pow(2, 31) - 1));
})();
