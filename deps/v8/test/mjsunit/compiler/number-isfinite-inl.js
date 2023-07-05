// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(f) {
  assertTrue(Number.isFinite(0));
  assertTrue(Number.isFinite(Number.MIN_VALUE));
  assertTrue(Number.isFinite(Number.MAX_VALUE));
  assertTrue(Number.isFinite(Number.MIN_SAFE_INTEGER));
  assertTrue(Number.isFinite(Number.MIN_SAFE_INTEGER - 13));
  assertTrue(Number.isFinite(Number.MAX_SAFE_INTEGER));
  assertTrue(Number.isFinite(Number.MAX_SAFE_INTEGER + 23));
  assertTrue(Number.isFinite(0));
  assertTrue(Number.isFinite(-1));
  assertTrue(Number.isFinite(123456));
  assertFalse(Number.isFinite(Number.NaN));
  assertFalse(Number.isFinite(Number.POSITIVE_INFINITY));
  assertFalse(Number.isFinite(Number.NEGATIVE_INFINITY));
  assertFalse(Number.isFinite(1 / 0));
  assertFalse(Number.isFinite(-1 / 0));
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();


function test2(f) {
  assertFalse(Number.isFinite({}));
  assertFalse(Number.isFinite("abc"));
  assertTrue(Number.isFinite(0));
  assertTrue(Number.isFinite(Number.MIN_VALUE));
  assertTrue(Number.isFinite(Number.MAX_VALUE));
  assertTrue(Number.isFinite(Number.MIN_SAFE_INTEGER));
  assertTrue(Number.isFinite(Number.MIN_SAFE_INTEGER - 13));
  assertTrue(Number.isFinite(Number.MAX_SAFE_INTEGER));
  assertTrue(Number.isFinite(Number.MAX_SAFE_INTEGER + 23));
  assertTrue(Number.isFinite(0));
  assertTrue(Number.isFinite(-1));
  assertTrue(Number.isFinite(123456));
  assertFalse(Number.isFinite(Number.NaN));
  assertFalse(Number.isFinite(Number.POSITIVE_INFINITY));
  assertFalse(Number.isFinite(Number.NEGATIVE_INFINITY));
  assertFalse(Number.isFinite(1 / 0));
  assertFalse(Number.isFinite(-1 / 0));
}

%PrepareFunctionForOptimization(test2);
test2();
test2();
%OptimizeFunctionOnNextCall(test2);
test2();
