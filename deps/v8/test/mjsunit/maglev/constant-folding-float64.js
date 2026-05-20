// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function subtract_zero(n) {
  return n - 0.0;
}
function multiply_one(n) {
  return n * 1.0;
}
function divide_one(n) {
  return n / 1.0;
}

for (let fun of [subtract_zero, multiply_one, divide_one]) {
  for (let [special_value, expected_output] of [[undefined, NaN],
                                                ["abcd", NaN],
                                                ["0", 0],
                                                [{}, NaN],
                                                [true, 1],
                                                [false, 0]]) {
    %ClearFunctionFeedback(fun);
    %PrepareFunctionForOptimization(fun);
    assertEquals(3.5, fun(3.5));

    %OptimizeMaglevOnNextCall(fun);
    assertEquals(-0.0, fun(-0.0));
    assertEquals(3.5, fun(3.5));
    assertEquals(NaN, fun(NaN));
    assertOptimized(fun);

    // Triggering deopt and checking the result
    assertEquals(expected_output, fun(special_value));
    assertUnoptimized(fun);
  }
}

// Add leads to different special values than sub/mul/div.
function add_minus_zero(n) {
  return n + -0.0;
}
for (let [special_value, expected_output] of [[undefined, NaN],
                                              ["abcd", "abcd0"],
                                              ["0", "00"],
                                              [{}, "[object Object]0"],
                                              [true, 1],
                                              [false, 0]]) {
  %ClearFunctionFeedback(add_minus_zero);
  %PrepareFunctionForOptimization(add_minus_zero);
  assertEquals(3.5, add_minus_zero(3.5));

  %OptimizeMaglevOnNextCall(add_minus_zero);
  assertEquals(-0.0, add_minus_zero(-0.0));
  assertEquals(3.5, add_minus_zero(3.5));
  assertEquals(NaN, add_minus_zero(NaN));
  assertOptimized(add_minus_zero);

  // Triggering deopt and checking the result
  assertEquals(expected_output, add_minus_zero(special_value));
  assertUnoptimized(add_minus_zero);
}

// Folding `+ 0.0` should not be allowed because if `n` is -0.0, it would remain
// -0.0 instead of becoming +0.0.
function add_zero(n) {
  return n + 0.0;
}

%PrepareFunctionForOptimization(add_zero);
assertEquals(3.5, add_zero(3.5));

%OptimizeMaglevOnNextCall(add_zero);
assertEquals(3.5, add_zero(3.5));
assertEquals(0, add_zero(-0.0));
assertOptimized(add_zero);

// Folding - -0.0 should not be allowed because `-0.0 - -0.0` is +0.0.
function subtract_minus_zero(n) {
  return n - -0.0;
}

%PrepareFunctionForOptimization(subtract_minus_zero);
assertEquals(3.5, subtract_minus_zero(3.5));

%OptimizeMaglevOnNextCall(subtract_minus_zero);
assertEquals(3.5, subtract_minus_zero(3.5));
assertEquals(0, subtract_minus_zero(-0.0));
assertNotEquals(-0.0, subtract_minus_zero(-0.0));
assertOptimized(subtract_minus_zero);
