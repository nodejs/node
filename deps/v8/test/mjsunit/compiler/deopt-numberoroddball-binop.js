// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
  function foo(x, y) { return x << y; }

  foo(1.1, 0.1);
  foo(0.1, 1.1);
  %OptimizeFunctionOnNextCall(foo);
  foo(undefined, 1.1);
  assertOptimized(foo);
  foo(1.1, undefined);
  assertOptimized(foo);
  foo(null, 1.1);
  assertOptimized(foo);
  foo(1.1, null);
  assertOptimized(foo);
  foo(true, 1.1);
  assertOptimized(foo);
  foo(1.1, true);
  assertOptimized(foo);
  foo(false, 1.1);
  assertOptimized(foo);
  foo(1.1, false);
  assertOptimized(foo);
})();

(function() {
  function foo(x, y) { return x >> y; }

  foo(1.1, 0.1);
  foo(0.1, 1.1);
  %OptimizeFunctionOnNextCall(foo);
  foo(undefined, 1.1);
  assertOptimized(foo);
  foo(1.1, undefined);
  assertOptimized(foo);
  foo(null, 1.1);
  assertOptimized(foo);
  foo(1.1, null);
  assertOptimized(foo);
  foo(true, 1.1);
  assertOptimized(foo);
  foo(1.1, true);
  assertOptimized(foo);
  foo(false, 1.1);
  assertOptimized(foo);
  foo(1.1, false);
  assertOptimized(foo);
})();

(function() {
  function foo(x, y) { return x >>> y; }

  foo(1.1, 0.1);
  foo(0.1, 1.1);
  %OptimizeFunctionOnNextCall(foo);
  foo(undefined, 1.1);
  assertOptimized(foo);
  foo(1.1, undefined);
  assertOptimized(foo);
  foo(null, 1.1);
  assertOptimized(foo);
  foo(1.1, null);
  assertOptimized(foo);
  foo(true, 1.1);
  assertOptimized(foo);
  foo(1.1, true);
  assertOptimized(foo);
  foo(false, 1.1);
  assertOptimized(foo);
  foo(1.1, false);
  assertOptimized(foo);
})();

(function() {
  function foo(x, y) { return x ^ y; }

  foo(1.1, 0.1);
  foo(0.1, 1.1);
  %OptimizeFunctionOnNextCall(foo);
  foo(undefined, 1.1);
  assertOptimized(foo);
  foo(1.1, undefined);
  assertOptimized(foo);
  foo(null, 1.1);
  assertOptimized(foo);
  foo(1.1, null);
  assertOptimized(foo);
  foo(true, 1.1);
  assertOptimized(foo);
  foo(1.1, true);
  assertOptimized(foo);
  foo(false, 1.1);
  assertOptimized(foo);
  foo(1.1, false);
  assertOptimized(foo);
})();

(function() {
  function foo(x, y) { return x | y; }

  foo(1.1, 0.1);
  foo(0.1, 1.1);
  %OptimizeFunctionOnNextCall(foo);
  foo(undefined, 1.1);
  assertOptimized(foo);
  foo(1.1, undefined);
  assertOptimized(foo);
  foo(null, 1.1);
  assertOptimized(foo);
  foo(1.1, null);
  assertOptimized(foo);
  foo(true, 1.1);
  assertOptimized(foo);
  foo(1.1, true);
  assertOptimized(foo);
  foo(false, 1.1);
  assertOptimized(foo);
  foo(1.1, false);
  assertOptimized(foo);
})();

(function() {
  function foo(x, y) { return x & y; }

  foo(1.1, 0.1);
  foo(0.1, 1.1);
  %OptimizeFunctionOnNextCall(foo);
  foo(undefined, 1.1);
  assertOptimized(foo);
  foo(1.1, undefined);
  assertOptimized(foo);
  foo(null, 1.1);
  assertOptimized(foo);
  foo(1.1, null);
  assertOptimized(foo);
  foo(true, 1.1);
  assertOptimized(foo);
  foo(1.1, true);
  assertOptimized(foo);
  foo(false, 1.1);
  assertOptimized(foo);
  foo(1.1, false);
  assertOptimized(foo);
})();
