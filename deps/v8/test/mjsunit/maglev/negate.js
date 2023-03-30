// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function negate(val) {
  return -val;
}

function test_negate_int32(value, expected) {
  // Warmup.
  %PrepareFunctionForOptimization(negate);
  %ClearFunctionFeedback(negate);
  negate(1, -1);
  %OptimizeMaglevOnNextCall(negate);
  assertEquals(expected, negate(value));
  assertTrue(isMaglevved(negate));

  %DeoptimizeFunction(negate);
  assertEquals(expected, negate(value));
}

test_negate_int32(1, -1);
test_negate_int32(-1, 1);
test_negate_int32(42, -42);
test_negate_int32(-42, 42);

function test_negate_float(value, expected) {
  // Warmup.
  %PrepareFunctionForOptimization(negate);
  %ClearFunctionFeedback(negate);
  negate(1.1, -1.1);
  %OptimizeMaglevOnNextCall(negate);
  assertEquals(expected, negate(value));
  assertTrue(isMaglevved(negate));

  %DeoptimizeFunction(negate);
  assertEquals(expected, negate(value));
}

test_negate_float(1.23, -1.23);
test_negate_float(-1.001, 1.001);
test_negate_float(42.42, -42.42);
test_negate_float(-42.42, 42.42);

const int32_max = Math.pow(2,30)-1;
const int32_min = -Math.pow(2,31);
test_negate_float(int32_max, -int32_max);
test_negate_float(int32_min, -int32_min);

function test_negate_int32_expect_deopt(value, expected) {
  // Warmup.
  %PrepareFunctionForOptimization(negate);
  %ClearFunctionFeedback(negate);
  negate(12, -12);
  %OptimizeMaglevOnNextCall(negate);
  assertEquals(expected, negate(value));
  assertFalse(isMaglevved(negate));
}

test_negate_int32_expect_deopt(0, -0);
test_negate_int32_expect_deopt(-0, 0);
test_negate_int32_expect_deopt(int32_min, -int32_min);
test_negate_int32_expect_deopt(-int32_min, int32_min);
test_negate_int32_expect_deopt(int32_max, -int32_max);
test_negate_int32_expect_deopt(-int32_max, int32_max);
