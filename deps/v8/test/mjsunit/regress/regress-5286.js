// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
function foo(x, y) {
  return x % y;
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(2, 2));
assertEquals(0, foo(4, 4));
%OptimizeFunctionOnNextCall(foo);
assertEquals(-0, foo(-8, 8));
})();

(function() {
function foo(x, y) {
  return x % y;
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(1, 1));
assertEquals(0, foo(2, 2));
%OptimizeFunctionOnNextCall(foo);
assertEquals(-0, foo(-3, 3));
})();

(function() {
function foo(x, y) {
  return x % y;
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(1, 1));
assertEquals(0, foo(2, 2));
%OptimizeFunctionOnNextCall(foo);
assertEquals(-0, foo(-2147483648, -1));
})();

(function() {
function foo(x, y) {
  return x % y;
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(1, 1));
assertEquals(0, foo(2, 2));
%OptimizeFunctionOnNextCall(foo);
assertEquals(-0, foo(-2147483648, -2147483648));
})();
