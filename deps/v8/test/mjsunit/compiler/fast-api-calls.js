// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file excercises basic fast API calls and enables fuzzing of this
// functionality.

// Flags: --turbo-fast-api-calls --allow-natives-syntax --opt
// --always-opt is disabled because we rely on particular feedback for
// optimizing to the fastest path.
// Flags: --no-always-opt

assertThrows(() => d8.test.fast_c_api());
const fast_c_api = new d8.test.fast_c_api();

// ----------- add_all -----------
// `add_all` has the following signature:
// double add_all(bool /*should_fallback*/, int32_t, uint32_t,
//   int64_t, uint64_t, float, double)

const max_safe_float = 2**24 - 1;
const add_all_result = -42 + 45 + Number.MIN_SAFE_INTEGER + Number.MAX_SAFE_INTEGER +
  max_safe_float * 0.5 + Math.PI;

function add_all(should_fallback = false) {
  return fast_c_api.add_all(should_fallback,
    -42, 45, Number.MIN_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
    max_safe_float * 0.5, Math.PI);
}

%PrepareFunctionForOptimization(add_all);
assertEquals(add_all_result, add_all());
%OptimizeFunctionOnNextCall(add_all);

if (fast_c_api.supports_fp_params) {
  // Test that regular call hits the fast path.
  fast_c_api.reset_counts();
  assertEquals(add_all_result, add_all());
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());

  // Test fallback to slow path.
  fast_c_api.reset_counts();
  assertEquals(add_all_result, add_all(true));
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(1, fast_c_api.slow_call_count());

  // Test that no fallback hits the fast path again.
  fast_c_api.reset_counts();
  assertEquals(add_all_result, add_all());
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
} else {
  // Test that calling with unsupported types hits the slow path.
  fast_c_api.reset_counts();
  assertEquals(add_all_result, add_all());
  assertEquals(0, fast_c_api.fast_call_count());
  assertEquals(1, fast_c_api.slow_call_count());
}

// ----------- Test add_all signature mismatche -----------
function add_all_mismatch() {
  return fast_c_api.add_all(false /*should_fallback*/,
    45, -42, Number.MAX_SAFE_INTEGER, max_safe_float * 0.5,
    Number.MIN_SAFE_INTEGER, Math.PI);
}

%PrepareFunctionForOptimization(add_all_mismatch);
const add_all_mismatch_result = add_all_mismatch();
%OptimizeFunctionOnNextCall(add_all_mismatch);

fast_c_api.reset_counts();
assertEquals(add_all_mismatch_result, add_all_mismatch());
assertEquals(1, fast_c_api.slow_call_count());
assertEquals(0, fast_c_api.fast_call_count());
// If the function was ever optimized to the fast path, it should
// have been deoptimized due to the argument types mismatch. If it
// wasn't optimized due to lack of support for FP params, it will
// stay optimized.
if (fast_c_api.supports_fp_params) {
  assertUnoptimized(add_all_mismatch);
}

// ----------- add_32bit_int -----------
// `add_32bit_int` has the following signature:
// int add_32bit_int(bool /*should_fallback*/, int32_t, uint32_t)

const add_32bit_int_result = -42 + 45;

function add_32bit_int(should_fallback = false) {
  return fast_c_api.add_32bit_int(should_fallback, -42, 45);
}

%PrepareFunctionForOptimization(add_32bit_int);
assertEquals(add_32bit_int_result, add_32bit_int());
%OptimizeFunctionOnNextCall(add_32bit_int);

// Test that regular call hits the fast path.
fast_c_api.reset_counts();
assertEquals(add_32bit_int_result, add_32bit_int());
assertEquals(1, fast_c_api.fast_call_count());
assertEquals(0, fast_c_api.slow_call_count());

// Test fallback to slow path.
fast_c_api.reset_counts();
assertEquals(add_32bit_int_result, add_32bit_int(true));
assertEquals(1, fast_c_api.fast_call_count());
assertEquals(1, fast_c_api.slow_call_count());

// Test that no fallback hits the fast path again.
fast_c_api.reset_counts();
assertEquals(add_32bit_int_result, add_32bit_int());
assertEquals(1, fast_c_api.fast_call_count());
assertEquals(0, fast_c_api.slow_call_count());

// ----------- Test various signature mismatches -----------
function add_32bit_int_mismatch(arg0, arg1, arg2, arg3) {
  return fast_c_api.add_32bit_int(arg0, arg1, arg2, arg3);
}

%PrepareFunctionForOptimization(add_32bit_int_mismatch);
assertEquals(add_32bit_int_result, add_32bit_int_mismatch(false, -42, 45));
%OptimizeFunctionOnNextCall(add_32bit_int_mismatch);

// Test that passing extra argument stays on the fast path.
fast_c_api.reset_counts();
assertEquals(add_32bit_int_result, add_32bit_int_mismatch(false, -42, 45, -42));
assertEquals(1, fast_c_api.fast_call_count());

// Test that passing wrong argument types stays on the fast path.
fast_c_api.reset_counts();
assertEquals(Math.round(-42 + 3.14), add_32bit_int_mismatch(false, -42, 3.14));
assertEquals(1, fast_c_api.fast_call_count());

// Test that passing too few argument falls down the slow path,
// because it's an argument type mismatch (undefined vs. int).
fast_c_api.reset_counts();
assertEquals(-42, add_32bit_int_mismatch(false, -42));
assertEquals(1, fast_c_api.slow_call_count());
assertEquals(0, fast_c_api.fast_call_count());
assertUnoptimized(add_32bit_int_mismatch);

// Test that the function can be optimized again.
%PrepareFunctionForOptimization(add_32bit_int_mismatch);
%OptimizeFunctionOnNextCall(add_32bit_int_mismatch);
fast_c_api.reset_counts();
assertEquals(add_32bit_int_result, add_32bit_int_mismatch(false, -42, 45));
assertEquals(1, fast_c_api.fast_call_count());
