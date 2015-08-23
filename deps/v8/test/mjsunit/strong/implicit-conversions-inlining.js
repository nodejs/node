// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

"use strict";

//******************************************************************************
// Number function declarations
function inline_add_strong(x, y) {
  "use strong";
  return x + y;
}

function inline_add_strong_outer(x, y) {
  return inline_add_strong(x, y);
}

function inline_sub_strong(x, y) {
  "use strong";
  return x - y;
}

function inline_sub_strong_outer(x, y) {
  return inline_sub_strong(x, y);
}

function inline_mul_strong(x, y) {
  "use strong";
  return x * y;
}

function inline_mul_strong_outer(x, y) {
  return inline_mul_strong(x, y);
}

function inline_div_strong(x, y) {
  "use strong";
  return x / y;
}

function inline_div_strong_outer(x, y) {
  return inline_div_strong(x, y);
}

function inline_mod_strong(x, y) {
  "use strong";
  return x % y;
}

function inline_mod_strong_outer(x, y) {
  return inline_mod_strong(x, y);
}

function inline_or_strong(x, y) {
  "use strong";
  return x | y;
}

function inline_or_strong_outer(x, y) {
  return inline_or_strong(x, y);
}

function inline_and_strong(x, y) {
  "use strong";
  return x & y;
}

function inline_and_strong_outer(x, y) {
  return inline_and_strong(x, y);
}

function inline_xor_strong(x, y) {
  "use strong";
  return x ^ y;
}

function inline_xor_strong_outer(x, y) {
  return inline_xor_strong(x, y);
}

function inline_shl_strong(x, y) {
  "use strong";
  return x << y;
}

function inline_shl_strong_outer(x, y) {
  return inline_shl_strong(x, y);
}

function inline_shr_strong(x, y) {
  "use strong";
  return x >> y;
}

function inline_shr_strong_outer(x, y) {
  return inline_shr_strong(x, y);
}

function inline_sar_strong(x, y) {
  "use strong";
  return x >>> y;
}

function inline_sar_strong_outer(x, y) {
  return inline_sar_strong(x, y);
}

function inline_less_strong(x, y) {
  "use strong";
  return x < y;
}

function inline_less_strong_outer(x, y) {
  return inline_less_strong(x, y);
}

function inline_greater_strong(x, y) {
  "use strong";
  return x > y;
}

function inline_greater_strong_outer(x, y) {
  return inline_greater_strong(x, y);
}

function inline_less_equal_strong(x, y) {
  "use strong";
  return x <= y;
}

function inline_less_equal_strong_outer(x, y) {
  return inline_less_equal_strong(x, y);
}

function inline_greater_equal_strong(x, y) {
  "use strong";
  return x >= y;
}

function inline_greater_equal_strong_outer(x, y) {
  return inline_greater_equal_strong(x, y);
}

function inline_add(x, y) {
  return x + y;
}

function inline_add_outer_strong(x, y) {
  "use strong";
  return inline_add(x, y);
}

function inline_sub(x, y) {
  return x - y;
}

function inline_sub_outer_strong(x, y) {
  "use strong";
  return inline_sub(x, y);
}

function inline_mul(x, y) {
  return x * y;
}

function inline_mul_outer_strong(x, y) {
  "use strong";
  return inline_mul(x, y);
}

function inline_div(x, y) {
  return x / y;
}

function inline_div_outer_strong(x, y) {
  "use strong";
  return inline_div(x, y);
}

function inline_mod(x, y) {
  return x % y;
}

function inline_mod_outer_strong(x, y) {
  "use strong";
  return inline_mod(x, y);
}

function inline_or(x, y) {
  return x | y;
}

function inline_or_outer_strong(x, y) {
  "use strong";
  return inline_or(x, y);
}

function inline_and(x, y) {
  return x & y;
}

function inline_and_outer_strong(x, y) {
  "use strong";
  return inline_and(x, y);
}

function inline_xor(x, y) {
  return x ^ y;
}

function inline_xor_outer_strong(x, y) {
  "use strong";
  return inline_xor(x, y);
}

function inline_shl(x, y) {
  return x << y;
}

function inline_shl_outer_strong(x, y) {
  "use strong";
  return inline_shl(x, y);
}

function inline_shr(x, y) {
  return x >> y;
}

function inline_shr_outer_strong(x, y) {
  "use strong";
  return inline_shr(x, y);
}

function inline_sar(x, y) {
  return x >>> y;
}

function inline_sar_outer_strong(x, y) {
  "use strong";
  return inline_sar(x, y);
}

function inline_less(x, y) {
  return x < y;
}

function inline_less_outer_strong(x, y) {
  "use strong";
  return inline_less(x, y);
}

function inline_greater(x, y) {
  return x > y;
}

function inline_greater_outer_strong(x, y) {
  "use strong";
  return inline_greater(x, y);
}

function inline_less_equal(x, y) {
  return x <= y;
}

function inline_less_equal_outer_strong(x, y) {
  "use strong";
  return inline_less_equal(x, y);
}

function inline_greater_equal(x, y) {
  return x >>> y;
}

function inline_greater_equal_outer_strong(x, y) {
  "use strong";
  return inline_greater_equal(x, y);
}

//******************************************************************************
// String function declarations
function inline_add_string_strong(x, y) {
  "use strong";
  return x + y;
}

function inline_add_string_strong_outer(x, y) {
  return inline_add_string_strong(x, y);
}

function inline_less_string_strong(x, y) {
  "use strong";
  return x < y;
}

function inline_less_string_strong_outer(x, y) {
  return inline_less_string_strong(x, y);
}

function inline_greater_string_strong(x, y) {
  "use strong";
  return x > y;
}

function inline_greater_string_strong_outer(x, y) {
  return inline_greater_string_strong(x, y);
}

function inline_less_equal_string_strong(x, y) {
  "use strong";
  return x <= y;
}

function inline_less_equal_string_strong_outer(x, y) {
  return inline_less_equal_string_strong(x, y);
}

function inline_greater_equal_string_strong(x, y) {
  "use strong";
  return x >= y;
}

function inline_greater_equal_string_strong_outer(x, y) {
  return inline_greater_equal_string_strong(x, y);
}

function inline_add_string(x, y) {
  return x + y;
}

function inline_add_string_outer_strong(x, y) {
  "use strong";
  return inline_add_string(x, y);
}

function inline_less_string(x, y) {
  return x < y;
}

function inline_less_string_outer_strong(x, y) {
  "use strong";
  return inline_less_string(x, y);
}

function inline_greater_string(x, y) {
  return x > y;
}

function inline_greater_string_outer_strong(x, y) {
  "use strong";
  return inline_greater_string(x, y);
}

function inline_less_equal_string(x, y) {
  return x <= y;
}

function inline_less_equal_string_outer_strong(x, y) {
  "use strong";
  return inline_less_equal_string(x, y);
}

function inline_greater_equal_string(x, y) {
  return x >= y;
}

function inline_greater_equal_string_outer_strong(x, y) {
  "use strong";
  return inline_greater_equal_string(x, y);
}


//******************************************************************************
// Testing
let strong_inner_funcs_num = [inline_add_strong_outer, inline_sub_strong_outer,
                              inline_mul_strong_outer, inline_div_strong_outer,
                              inline_mod_strong_outer, inline_or_strong_outer,
                              inline_and_strong_outer, inline_xor_strong_outer,
                              inline_shl_strong_outer, inline_shr_strong_outer,
                              inline_less_strong_outer,
                              inline_greater_strong_outer,
                              inline_less_equal_strong_outer,
                              inline_greater_equal_strong_outer];

let strong_outer_funcs_num = [inline_add_outer_strong, inline_sub_outer_strong,
                              inline_mul_outer_strong, inline_div_outer_strong,
                              inline_mod_outer_strong, inline_or_outer_strong,
                              inline_and_outer_strong, inline_xor_outer_strong,
                              inline_shl_outer_strong, inline_shr_outer_strong,
                              inline_less_outer_strong,
                              inline_greater_outer_strong,
                              inline_less_equal_outer_strong,
                              inline_greater_equal_outer_strong];

let strong_inner_funcs_string = [inline_add_string_strong_outer,
                                 inline_less_string_strong_outer,
                                 inline_greater_string_strong_outer,
                                 inline_less_equal_string_strong_outer,
                                 inline_greater_equal_string_strong_outer];

let strong_outer_funcs_string = [inline_add_string_outer_strong,
                                 inline_less_string_outer_strong,
                                 inline_greater_string_outer_strong,
                                 inline_less_equal_string_outer_strong,
                                 inline_greater_equal_string_outer_strong];

for (let strong_inner_func of strong_inner_funcs_num) {
  assertThrows(function(){strong_inner_func(1, {})}, TypeError);
  for (var i = 0; i < 100; i++) {
    strong_inner_func(1, 2);
  }
  %OptimizeFunctionOnNextCall(strong_inner_func);
  assertThrows(function(){strong_inner_func(1, {})}, TypeError);
}

for (let strong_outer_func of strong_outer_funcs_num) {
  assertDoesNotThrow(function(){strong_outer_func(1, {})});
  for (var i = 0; i < 100; i++) {
    strong_outer_func(1, 2);
  }
  %OptimizeFunctionOnNextCall(strong_outer_func);
  assertDoesNotThrow(function(){strong_outer_func(1, {})});
}

for (let strong_inner_func of strong_inner_funcs_string) {
  assertThrows(function(){strong_inner_func("foo", {})}, TypeError);
  for (var i = 0; i < 100; i++) {
    strong_inner_func("foo", "bar");
  }
  %OptimizeFunctionOnNextCall(strong_inner_func);
  assertThrows(function(){strong_inner_func("foo", {})}, TypeError);
}

for (let strong_outer_func of strong_outer_funcs_string) {
  assertDoesNotThrow(function(){strong_outer_func("foo", {})});
  for (var i = 0; i < 100; i++) {
    strong_outer_func("foo", "bar");
  }
  %OptimizeFunctionOnNextCall(strong_outer_func);
  assertDoesNotThrow(function(){strong_outer_func("foo", {})});
}
