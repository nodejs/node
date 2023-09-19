// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(f) {
  assertTrue(f(0));
  assertTrue(f(Number.MIN_SAFE_INTEGER));
  assertFalse(f(Number.MIN_SAFE_INTEGER - 13));
  assertTrue(f(Number.MIN_SAFE_INTEGER + 13));
  assertTrue(f(Number.MAX_SAFE_INTEGER));
  assertFalse(f(Number.MAX_SAFE_INTEGER + 23));
  assertTrue(f(Number.MAX_SAFE_INTEGER - 23));
  assertFalse(f(Number.MIN_VALUE));
  assertFalse(f(Number.MAX_VALUE));
  assertFalse(f(Number.NaN));
  assertFalse(f(Number.POSITIVE_INFINITY));
  assertFalse(f(Number.NEGATIVE_INFINITY));
  assertFalse(f(1 / 0));
  assertFalse(f(-1 / 0));
  assertFalse(f(Number.EPSILON));

  var near_upper = Math.pow(2, 52);
  assertTrue(f(near_upper));
  assertFalse(f(2 * near_upper));
  assertTrue(f(2 * near_upper - 1));
  assertTrue(f(2 * near_upper - 2));
  assertFalse(f(2 * near_upper + 1));
  assertFalse(f(2 * near_upper + 2));
  assertFalse(f(2 * near_upper + 7));

  var near_lower = -near_upper;
  assertTrue(f(near_lower));
  assertFalse(f(2 * near_lower));
  assertTrue(f(2 * near_lower + 1));
  assertTrue(f(2 * near_lower + 2));
  assertFalse(f(2 * near_lower - 1));
  assertFalse(f(2 * near_lower - 2));
  assertFalse(f(2 * near_lower - 7));
}

// Check that the NumberIsSafeInteger simplified operator in
// TurboFan does the right thing.
function NumberIsSafeInteger(x) { return Number.isSafeInteger(+x); }
%PrepareFunctionForOptimization(NumberIsSafeInteger);
test(NumberIsSafeInteger);
test(NumberIsSafeInteger);
%OptimizeFunctionOnNextCall(NumberIsSafeInteger);
test(NumberIsSafeInteger);

// Check that the ObjectIsSafeInteger simplified operator in
// TurboFan does the right thing as well (i.e. when TurboFan
// is not able to tell statically that the inputs are numbers).
function ObjectIsSafeInteger(x) { return Number.isSafeInteger(x); }
%PrepareFunctionForOptimization(ObjectIsSafeInteger);
test(ObjectIsSafeInteger);
test(ObjectIsSafeInteger);
%OptimizeFunctionOnNextCall(ObjectIsSafeInteger);
test(ObjectIsSafeInteger);
