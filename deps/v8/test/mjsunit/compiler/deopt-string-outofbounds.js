// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

var s = "12345";

(function() {
  function foo() { return s[5]; }

  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo();
  assertOptimized(foo);
})();

(function() {
  function foo(i) { return s[i]; }

  %PrepareFunctionForOptimization(foo);
  foo(0);
  foo(1);
  %OptimizeFunctionOnNextCall(foo);
  foo(5);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo(5);
  assertOptimized(foo);
})();

(function() {
  function foo(s) { return s[5]; }

  %PrepareFunctionForOptimization(foo);
  foo(s);
  foo(s);
  %OptimizeFunctionOnNextCall(foo);
  foo(s);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo(s);
  assertOptimized(foo);
})();

(function() {
  function foo(s, i) { return s[i]; }

  %PrepareFunctionForOptimization(foo);
  foo(s, 0);
  foo(s, 1);
  %OptimizeFunctionOnNextCall(foo);
  foo(s, 5);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo(s, 5);
  assertOptimized(foo);
})();
