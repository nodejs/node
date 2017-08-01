// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

(function() {
  function foo(x) {
    x = x >>> 0;
    var y = 0 - 2147483648;
    return x + y;
  }

  assertEquals(-2147483648, foo(0));
  assertEquals(0, foo(2147483648));
  assertEquals(2147483647, foo(4294967295));
  assertEquals(-2147483648, foo(0));
  assertEquals(0, foo(2147483648));
  assertEquals(2147483647, foo(4294967295));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-2147483648, foo(0));
  assertEquals(0, foo(2147483648));
  assertEquals(2147483647, foo(4294967295));
  assertOptimized(foo);
})();

(function() {
  function foo(x) {
    x = x >>> 0;
    var y = 2147483648;
    return x - y;
  }

  assertEquals(-2147483648, foo(0));
  assertEquals(0, foo(2147483648));
  assertEquals(2147483647, foo(4294967295));
  assertEquals(-2147483648, foo(0));
  assertEquals(0, foo(2147483648));
  assertEquals(2147483647, foo(4294967295));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-2147483648, foo(0));
  assertEquals(0, foo(2147483648));
  assertEquals(2147483647, foo(4294967295));
  assertOptimized(foo);
})();

(function() {
  function foo(x) {
    x = x | 0;
    var y = 2147483648;
    return x + y;
  }

  assertEquals(2147483648, foo(0));
  assertEquals(0, foo(-2147483648));
  assertEquals(4294967295, foo(2147483647));
  assertEquals(2147483648, foo(0));
  assertEquals(0, foo(-2147483648));
  assertEquals(4294967295, foo(2147483647));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2147483648, foo(0));
  assertEquals(0, foo(-2147483648));
  assertEquals(4294967295, foo(2147483647));
  assertOptimized(foo);
})();

(function() {
  function foo(x) {
    x = x | 0;
    var y = 0 - 2147483648;
    return x - y;
  }

  assertEquals(2147483648, foo(0));
  assertEquals(0, foo(-2147483648));
  assertEquals(4294967295, foo(2147483647));
  assertEquals(2147483648, foo(0));
  assertEquals(0, foo(-2147483648));
  assertEquals(4294967295, foo(2147483647));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2147483648, foo(0));
  assertEquals(0, foo(-2147483648));
  assertEquals(4294967295, foo(2147483647));
  assertOptimized(foo);
})();

(function() {
  function foo(x) {
    x = x | 0;
    var y = -0;
    return x + y;
  }

  assertEquals(2147483647, foo(2147483647));
  assertEquals(-2147483648, foo(-2147483648));
  assertEquals(0, foo(0));
  assertEquals(2147483647, foo(2147483647));
  assertEquals(-2147483648, foo(-2147483648));
  assertEquals(0, foo(0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2147483647, foo(2147483647));
  assertEquals(-2147483648, foo(-2147483648));
  assertEquals(0, foo(0));
  assertOptimized(foo);
})();

(function() {
  function foo(x) {
    var y = (x < 0) ? 4294967295 : 4294967296;
    var z = (x > 0) ? 2147483647 : 2147483648;
    return y - z;
  }

  assertEquals(2147483647, foo(-1));
  assertEquals(2147483648, foo(0));
  assertEquals(2147483649, foo(1));
  assertEquals(2147483647, foo(-1));
  assertEquals(2147483648, foo(0));
  assertEquals(2147483649, foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2147483647, foo(-1));
  assertEquals(2147483648, foo(0));
  assertEquals(2147483649, foo(1));
  assertOptimized(foo);
})();
