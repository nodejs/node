// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function gen_sarl_smi(y) {
  return function sarl_smi(x) { return x >> y; };
}

function sarl_test(lhs, rhs, expected_result) {
  const sarl = gen_sarl_smi(rhs);

  // Warmup.
  %PrepareFunctionForOptimization(sarl);
  %ClearFunctionFeedback(sarl);
  sarl(1);
  %OptimizeMaglevOnNextCall(sarl);

  assertEquals(expected_result, sarl(lhs));
  assertTrue(isMaglevved(sarl));

  %DeoptimizeFunction(sarl);
  assertEquals(expected_result, sarl(lhs));
}

function sarl_test_expect_deopt(lhs, rhs, expected_result) {
  const sarl = gen_sarl_smi(rhs);

  // Warmup.
  %PrepareFunctionForOptimization(sarl);
  %ClearFunctionFeedback(sarl);
  sarl(1);
  %OptimizeMaglevOnNextCall(sarl);

  assertEquals(expected_result, sarl(lhs));
  assertFalse(isMaglevved(sarl));
}

sarl_test(8, 2, 2);
sarl_test(-8, 2, -2);
sarl_test(-8, 0, -8);
sarl_test(8, 10, 0);
sarl_test(8, 33, 4);
sarl_test_expect_deopt(0xFFFFFFFF, 0x3FFFFFFF, -1);
