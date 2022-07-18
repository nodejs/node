// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan


// smi == boolean
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(2, true));
  assertTrue(foo(1, true));
  assertTrue(foo(0, false));
  assertFalse(foo(2, false));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(2, true));
  assertTrue(foo(1, true));
  assertTrue(foo(0, false));
  assertFalse(foo(2, false));
  assertOptimized(foo);

  assertFalse(foo(0, null));
  assertUnoptimized(foo);
})();


// boolean == smi
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(true, 2));
  assertTrue(foo(true, 1));
  assertTrue(foo(false, 0));
  assertFalse(foo(false, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(true, 2));
  assertTrue(foo(true, 1));
  assertTrue(foo(false, 0));
  assertFalse(foo(false, 2));
  assertOptimized(foo);

  assertFalse(foo(null, 0));
  assertUnoptimized(foo);
})();


// smi == undefined
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(2, undefined));
  assertFalse(foo(0, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(2, undefined));
  assertFalse(foo(0, undefined));
  assertOptimized(foo);
})();


// undefined == smi
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(undefined, 2));
  assertFalse(foo(undefined, 0));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(undefined, 2));
  assertFalse(foo(undefined, 0));
  assertOptimized(foo);
})();


// smi == null
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(2, null));
  assertFalse(foo(0, null));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(2, null));
  assertFalse(foo(0, null));
  assertOptimized(foo);
})();


// null == smi
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(null, 2));
  assertFalse(foo(null, 0));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(null, 2));
  assertFalse(foo(null, 0));
  assertOptimized(foo);
})();


// smi == oddball
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(2, null));
  assertFalse(foo(0, undefined));
  assertFalse(foo(0, true));
  assertTrue(foo(1, true));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(2, null));
  assertFalse(foo(0, undefined));
  assertFalse(foo(0, true));
  assertTrue(foo(1, true));
  assertOptimized(foo);
})();


// oddball == smi
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(null, 2));
  assertFalse(foo(undefined, 0));
  assertFalse(foo(true, 0));
  assertTrue(foo(true, 1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(null, 2));
  assertFalse(foo(undefined, 0));
  assertFalse(foo(true, 0));
  assertTrue(foo(true, 1));
  assertOptimized(foo);
})();


// number == boolean
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(-2.8, true));
  assertTrue(foo(1, true));
  assertTrue(foo(-0.0, false));
  assertFalse(foo(2, false));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-2.8, true));
  assertTrue(foo(1, true));
  assertTrue(foo(-0.0, false));
  assertFalse(foo(2, false));
  assertOptimized(foo);

  assertFalse(foo(0, null));
  assertUnoptimized(foo);
})();


// boolean == number
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(true, -2.8));
  assertTrue(foo(true, 1));
  assertTrue(foo(false, -0.0));
  assertFalse(foo(false, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(true, -2.8));
  assertTrue(foo(true, 1));
  assertTrue(foo(false, -0.0));
  assertFalse(foo(false, 2));
  assertOptimized(foo);

  assertFalse(foo(null, 0));
  assertUnoptimized(foo);
})();


// number == undefined
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(-2.8, undefined));
  assertFalse(foo(-0.0, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-2.8, undefined));
  assertFalse(foo(-0.0, undefined));
  assertOptimized(foo);
})();


// undefined == number
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(undefined, -2.8));
  assertFalse(foo(undefined, -0.0));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(undefined, -2.8));
  assertFalse(foo(undefined, -0.0));
  assertOptimized(foo);
})();


// number == null
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(-2.8, null));
  assertFalse(foo(-0.0, null));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-2.8, null));
  assertFalse(foo(-0.0, null));
  assertOptimized(foo);
})();


// null == number
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(null, -2.8));
  assertFalse(foo(null, -0.0));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(null, -2.8));
  assertFalse(foo(null, -0.0));
  assertOptimized(foo);
})();


// number == oddball
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(-2.8, null));
  assertFalse(foo(-0.0, undefined));
  assertFalse(foo(0, true));
  assertTrue(foo(1.0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(-2.8, null));
  assertFalse(foo(-0.0, undefined));
  assertFalse(foo(0, true));
  assertTrue(foo(1.0, true));
  assertOptimized(foo);
})();


// oddball == number
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(null, -2.8));
  assertFalse(foo(undefined, -0.0));
  assertFalse(foo(true, 0));
  assertTrue(foo(true, 1.0));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(null, -2.8));
  assertFalse(foo(undefined, -0.0));
  assertFalse(foo(true, 0));
  assertTrue(foo(true, 1.0));
  assertOptimized(foo);
})();


// oddball == oddball
(function() {
  function foo(a, b) { return a == b; }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(null, null));
  assertTrue(foo(undefined, undefined));
  assertTrue(foo(false, false));
  assertTrue(foo(true, true));
  assertTrue(foo(undefined, null));
  assertFalse(foo(undefined, false));
  assertFalse(foo(null, false));
  assertFalse(foo(true, undefined));
  assertFalse(foo(true, null));
  assertFalse(foo(true, false));

  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(null, null));
  assertOptimized(foo);
  assertTrue(foo(undefined, undefined));
  assertTrue(foo(false, false));
  assertTrue(foo(true, true));
  assertTrue(foo(undefined, null));
  assertFalse(foo(undefined, false));
  assertFalse(foo(null, false));
  assertFalse(foo(true, undefined));
  assertFalse(foo(true, null));
  assertFalse(foo(true, false));
  assertOptimized(foo);
})();
