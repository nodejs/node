// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function shrl(x, y) {
  return x >>> y;
}

function shrl_test(lhs, rhs, expected_result) {
  // Warmup.
  %PrepareFunctionForOptimization(shrl);
  %ClearFunctionFeedback(shrl);
  shrl(1, 1);
  %OptimizeMaglevOnNextCall(shrl);

  assertEquals(expected_result, shrl(lhs, rhs));
  assertTrue(isMaglevved(shrl));

  %DeoptimizeFunction(shrl);
  assertEquals(expected_result, shrl(lhs, rhs));
}


function shrl_test_expect_deopt(lhs, rhs, expected_result) {
  // Warmup.
  %PrepareFunctionForOptimization(shrl);
  %ClearFunctionFeedback(shrl);
  shrl(1, 1);
  %OptimizeMaglevOnNextCall(shrl);

  assertEquals(expected_result, shrl(lhs, rhs));
  assertFalse(isMaglevved(shrl));
}

shrl_test(8, 2, 2);
shrl_test_expect_deopt(-1, 1, 2147483647);
shrl_test(-8, 2, 1073741822);
shrl_test_expect_deopt(-8, 0, 4294967288);
shrl_test_expect_deopt(-892396978, 0, 3402570318);
shrl_test(8, 10, 0);
shrl_test(8, 33, 4);
shrl_test_expect_deopt(0xFFFFFFFF, 0x3FFFFFFF, 1);
