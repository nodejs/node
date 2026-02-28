// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function max0() {
  return Math.max();
}
%PrepareFunctionForOptimization(max0);

max0();

%OptimizeMaglevOnNextCall(max0);
assertEquals(-Infinity, max0());

function max1(a) {
  return Math.max(a);
}
%PrepareFunctionForOptimization(max1);

function foo1() {
  // a is an Int32 constant.
  return max1(-8);
}
%PrepareFunctionForOptimization(foo1);

foo1();

%OptimizeMaglevOnNextCall(foo1);
assertEquals(-8, foo1());

function max2(a, b) {
  return Math.max(a, b);
}
%PrepareFunctionForOptimization(max2);

function foo2() {
  // a and b are Int32 constants.
  return max2(0, 1);
}
%PrepareFunctionForOptimization(foo2);

foo2();

%OptimizeMaglevOnNextCall(foo2);
assertEquals(1, foo2());

function max3(a, b, c) {
  return Math.max(a, b, c);
}
%PrepareFunctionForOptimization(max3);

function foo3() {
  // a, b and c are Int32 constants.
  return max3(0, 1, -5);
}
%PrepareFunctionForOptimization(foo3);

foo3();

%OptimizeMaglevOnNextCall(foo3);
assertEquals(1, foo3());
