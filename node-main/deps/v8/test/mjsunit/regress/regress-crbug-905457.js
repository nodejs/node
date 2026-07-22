// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
function foo(x) {
  return Math.abs(Math.min(+x, 0));
};
%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo());
assertEquals(NaN, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
})();

(function() {
function foo(x) {
  return Math.abs(Math.min(-x, 0));
};
%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo());
assertEquals(NaN, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
})();

(function() {
function foo(x) {
  return Math.abs(Math.max(0, +x));
};
%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo());
assertEquals(NaN, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
})();

(function() {
function foo(x) {
  return Math.abs(Math.max(0, -x));
};
%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo());
assertEquals(NaN, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
})();
