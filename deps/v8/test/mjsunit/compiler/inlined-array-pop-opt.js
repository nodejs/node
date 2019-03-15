// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
  function foo(a) { return a.pop(); }

  var x = {};
  var a = [x,x,];

  %PrepareFunctionForOptimization(foo);
  assertEquals(x, foo(a));
  assertEquals(x, foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(a));
  assertOptimized(foo);
})();

(function() {
  function foo(a) { return a.pop(); }

  var x = 0;
  var a = [x,x,];

  %PrepareFunctionForOptimization(foo);
  assertEquals(x, foo(a));
  assertEquals(x, foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(a));
  assertOptimized(foo);
})();

(function() {
  function foo(a) { return a.pop(); }

  var x = 0;
  var a = [x,x,x];

  %PrepareFunctionForOptimization(foo);
  assertEquals(x, foo(a));
  assertEquals(x, foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(x, foo(a));
  assertOptimized(foo);
})();

(function() {
  function foo(a) { return a.pop(); }

  var x = {};
  var a = [x,x,x];

  %PrepareFunctionForOptimization(foo);
  assertEquals(x, foo(a));
  assertEquals(x, foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(x, foo(a));
  assertOptimized(foo);
})();

(function() {
  function foo(a) { return a.pop(); }

  var a = [,,];

  %PrepareFunctionForOptimization(foo);
  assertEquals(undefined, foo(a));
  assertEquals(undefined, foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(a));
  assertOptimized(foo);
})();

(function() {
  var pop = Array.prototype.pop;

  function foo(a) { return a.pop(); }

  var a = [1, 2, 3];

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(a));
  assertEquals(2, foo(a));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(a));
  assertOptimized(foo);
})();
