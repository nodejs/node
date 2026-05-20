// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function min0() {
  return Math.min();
}
%PrepareFunctionForOptimization(min0);

min0();

%OptimizeMaglevOnNextCall(min0);
assertEquals(Infinity, min0());

function min1(a) {
  return Math.min(a);
}
%PrepareFunctionForOptimization(min1);

function foo1() {
  // a is an Int32 constant.
  return min1(-8);
}
%PrepareFunctionForOptimization(foo1);

foo1();

%OptimizeMaglevOnNextCall(foo1);
assertEquals(-8, foo1());

function min2(a, b) {
  return Math.min(a, b);
}
%PrepareFunctionForOptimization(min2);

function foo2() {
  // a and b are Int32 constants.
  return min2(0, 1);
}
%PrepareFunctionForOptimization(foo2);

foo2();

%OptimizeMaglevOnNextCall(foo2);
assertEquals(0, foo2());

function min3(a, b, c) {
  return Math.min(a, b, c);
}
%PrepareFunctionForOptimization(min3);

function foo3() {
  // a, b and c are Int32 constants.
  return min3(0, 1, -5);
}
%PrepareFunctionForOptimization(foo3);

foo3();

%OptimizeMaglevOnNextCall(foo3);
assertEquals(-5, foo3());
