// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
  function foo(a, v) {
    a[0] = v & 0xff;
  }

  var a = new Uint8ClampedArray(4);
  foo(a, 1);
  foo(a, 2);
  %OptimizeFunctionOnNextCall(foo);
  foo(a, 256);
  assertOptimized(foo);
  assertEquals(0, a[0]);
})();

(function() {
  function foo(a, v) {
    a[0] = v >>> 0;
  }

  var a = new Uint8ClampedArray(4);
  foo(a, 1);
  foo(a, 2);
  %OptimizeFunctionOnNextCall(foo);
  foo(a, 256);
  assertOptimized(foo);
  assertEquals(255, a[0]);
})();

(function() {
  function foo(a, v) {
    a[0] = v | 0;
  }

  var a = new Uint8ClampedArray(4);
  foo(a, 1);
  foo(a, 2);
  %OptimizeFunctionOnNextCall(foo);
  foo(a, 256);
  assertOptimized(foo);
  assertEquals(255, a[0]);
  foo(a, -1);
  assertOptimized(foo);
  assertEquals(0, a[0]);
})();

(function() {
  function foo(a, v) {
    a[0] = v;
  }

  var a = new Uint8ClampedArray(4);
  foo(a, 1);
  foo(a, 2);
  %OptimizeFunctionOnNextCall(foo);
  foo(a, Infinity);
  assertOptimized(foo);
  assertEquals(255, a[0]);
  foo(a, -Infinity);
  assertOptimized(foo);
  assertEquals(0, a[0]);
  foo(a, 0.5);
  assertOptimized(foo);
  assertEquals(0, a[0]);
  foo(a, 1.5);
  assertOptimized(foo);
  assertEquals(2, a[0]);
})();
