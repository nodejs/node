// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testMax1(b) {
  const max = Math.max(-1, b ? -0 : 1);
  return Object.is(max, -0);
}
%PrepareFunctionForOptimization(testMax1);
assertTrue(testMax1(true));
assertTrue(testMax1(true));
%OptimizeFunctionOnNextCall(testMax1);
assertTrue(testMax1(true));

function testMax2(b) {
  const max = Math.max(b ? -0 : 1, -1);
  return Object.is(max, -0);
}
%PrepareFunctionForOptimization(testMax2);
assertTrue(testMax2(true));
assertTrue(testMax2(true));
%OptimizeFunctionOnNextCall(testMax2);
assertTrue(testMax2(true));

function testMin1(b) {
  const min = Math.min(1, b ? -0 : -1);
  return Object.is(min, -0);
}
%PrepareFunctionForOptimization(testMin1);
assertTrue(testMin1(true));
assertTrue(testMin1(true));
%OptimizeFunctionOnNextCall(testMin1);
assertTrue(testMin1(true));

function testMin2(b) {
  const min = Math.min(b ? -0 : -1, 1);
  return Object.is(min, -0);
}
%PrepareFunctionForOptimization(testMin2);
assertTrue(testMin2(true));
assertTrue(testMin2(true));
%OptimizeFunctionOnNextCall(testMin2);
assertTrue(testMin2(true));
