// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
  assertTrue(Number.isInteger(0));
  assertFalse(Number.isInteger(Number.MIN_VALUE));
  assertTrue(Number.isInteger(Number.MAX_VALUE));
  assertTrue(Number.isInteger(Number.MIN_SAFE_INTEGER));
  assertTrue(Number.isInteger(Number.MIN_SAFE_INTEGER - 13));
  assertTrue(Number.isInteger(Number.MAX_SAFE_INTEGER));
  assertTrue(Number.isInteger(Number.MAX_SAFE_INTEGER + 23));
  assertTrue(Number.isInteger(0));
  assertTrue(Number.isInteger(-1));
  assertTrue(Number.isInteger(123456));
  assertFalse(Number.isInteger(Number.NaN));
  assertFalse(Number.isInteger(Number.POSITIVE_INFINITY));
  assertFalse(Number.isInteger(Number.NEGATIVE_INFINITY));
  assertFalse(Number.isInteger(1 / 0));
  assertFalse(Number.isInteger(-1 / 0));
  assertFalse(Number.isInteger(Number.EPSILON));
}

test();
test();
%OptimizeFunctionOnNextCall(test);
test();


function test2() {
  assertTrue(Number.isInteger(0));
  assertFalse(Number.isInteger(Number.MIN_VALUE));
  assertTrue(Number.isInteger(Number.MAX_VALUE));
  assertTrue(Number.isInteger(Number.MIN_SAFE_INTEGER));
  assertTrue(Number.isInteger(Number.MIN_SAFE_INTEGER - 13));
  assertTrue(Number.isInteger(Number.MAX_SAFE_INTEGER));
  assertTrue(Number.isInteger(Number.MAX_SAFE_INTEGER + 23));
  assertTrue(Number.isInteger(0));
  assertTrue(Number.isInteger(-1));
  assertTrue(Number.isInteger(123456));
  assertFalse(Number.isInteger(Number.NaN));
  assertFalse(Number.isInteger(Number.POSITIVE_INFINITY));
  assertFalse(Number.isInteger(Number.NEGATIVE_INFINITY));
  assertFalse(Number.isInteger(1 / 0));
  assertFalse(Number.isInteger(-1 / 0));
  assertFalse(Number.isInteger(Number.EPSILON));
}

test2();
test2();
%OptimizeFunctionOnNextCall(test2);
test2();
