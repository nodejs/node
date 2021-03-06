// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
  function foo(o) { o["x"] = 1; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(undefined));
  assertThrows(() => foo(undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(undefined));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { o["x"] = 1; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(null));
  assertThrows(() => foo(null));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(null));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { return o["x"]; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(undefined));
  assertThrows(() => foo(undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(undefined));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { return o["x"]; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(null));
  assertThrows(() => foo(null));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(null));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { o.x = 1; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(undefined));
  assertThrows(() => foo(undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(undefined));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { o.x = 1; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(null));
  assertThrows(() => foo(null));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(null));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { return o.x; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(undefined));
  assertThrows(() => foo(undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(undefined));
  assertOptimized(foo);
})();

(function() {
  function foo(o) { return o.x; }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(null));
  assertThrows(() => foo(null));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(null));
  assertOptimized(foo);
})();
