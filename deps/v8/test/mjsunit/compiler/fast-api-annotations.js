// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests exercise WebIDL annotations support in the fast API.

// Flags: --turbo-fast-api-calls --expose-fast-api --allow-natives-syntax --turbofan
// TODO(mslekova): Implement support for TryTruncateFloat64ToInt32.
// Flags: --no-turboshaft
// --always-turbofan is disabled because we rely on particular feedback for
// optimizing to the fastest path.
// Flags: --no-always-turbofan
// The test relies on optimizing/deoptimizing at predictable moments, so
// it's not suitable for deoptimization fuzzing.
// Flags: --deopt-every-n-times=0

const fast_c_api = new d8.test.FastCAPI();

// ----------- add_all_annotate_enforce_range -----------
// `add_all_annotate_enforce_range` has the following signature:
// double add_all_annotate_enforce_range(bool /*should_fallback*/,
//   int32_t, uint32_t,
//   int64_t, uint64_t)

const limits_params = [
  -(2 ** 31),  // i32
  2 ** 32 - 1,   // u32
  Number.MIN_SAFE_INTEGER,  // i64
  Number.MAX_SAFE_INTEGER   // u64
];
const limits_result = limits_params[0] + limits_params[1] + limits_params[2] + limits_params[3];

function add_all_annotate_enforce_range(params, should_fallback = false) {
  return fast_c_api.add_all_annotate_enforce_range(should_fallback,
    params[0], params[1], params[2], params[3]);
}

%PrepareFunctionForOptimization(add_all_annotate_enforce_range);
assertEquals(limits_result, add_all_annotate_enforce_range(limits_params));
%OptimizeFunctionOnNextCall(add_all_annotate_enforce_range);
assertEquals(limits_result, add_all_annotate_enforce_range(limits_params));

const min_int32 = -(2 ** 31);
const max_int32 = 2 ** 31 - 1;
const min_uint32 = 0;
const max_uint32 = 2 ** 32 - 1;

// ----------- enforce_range_compare -----------
// `enforce_range_compare` has the following signature:
// bool enforce_range_compare(bool /*in_range*/,
//   double, integer_type)
// where integer_type = {int32_t, uint32_t, int64_t, uint64_t}

// ----------- i32 -----------
function compare_i32(in_range, arg) {
  return fast_c_api.enforce_range_compare_i32(in_range, arg, arg);
}

%PrepareFunctionForOptimization(compare_i32);
assertFalse(compare_i32(true, 123));
%OptimizeFunctionOnNextCall(compare_i32);
assertTrue(compare_i32(true, 123));
assertTrue(compare_i32(true, -0.5));
assertTrue(compare_i32(true, 0.5));
assertTrue(compare_i32(true, 1.5));
assertTrue(compare_i32(true, -(2 ** 31)));
assertTrue(compare_i32(true, 2 ** 31 - 1));
assertThrows(() => compare_i32(false, -(2 ** 32)));
assertThrows(() => compare_i32(false, -(2 ** 32 + 1)));
assertThrows(() => compare_i32(false, 2 ** 32));
assertThrows(() => compare_i32(false, 2 ** 32 + 3.15));
assertThrows(() => compare_i32(false, Number.MIN_SAFE_INTEGER));
assertThrows(() => compare_i32(false, Number.MAX_SAFE_INTEGER));

// ----------- u32 -----------
function compare_u32(in_range, arg) {
  return fast_c_api.enforce_range_compare_u32(in_range, arg, arg);
}

%PrepareFunctionForOptimization(compare_u32);
assertFalse(compare_u32(true, 123));
%OptimizeFunctionOnNextCall(compare_u32);
assertTrue(compare_u32(true, 123));
assertTrue(compare_u32(true, 0));
assertTrue(compare_u32(true, -0.5));
assertTrue(compare_u32(true, 0.5));
assertTrue(compare_u32(true, 2 ** 32 - 1));
assertThrows(() => compare_u32(false, -(2 ** 31)));
assertThrows(() => compare_u32(false, 2 ** 32));
assertThrows(() => compare_u32(false, -1));
assertThrows(() => compare_u32(false, -1.5));
assertThrows(() => compare_u32(false, Number.MIN_SAFE_INTEGER));
assertThrows(() => compare_u32(false, Number.MAX_SAFE_INTEGER));

// ----------- i64 -----------
function compare_i64(in_range, arg) {
  return fast_c_api.enforce_range_compare_i64(in_range, arg, arg);
}

%PrepareFunctionForOptimization(compare_i64);
assertFalse(compare_i64(true, 123));
%OptimizeFunctionOnNextCall(compare_i64);
assertTrue(compare_i64(true, 123));
assertTrue(compare_i64(true, -0.5));
assertTrue(compare_i64(true, 0.5));
assertTrue(compare_i64(true, 1.5));
assertTrue(compare_i64(true, -(2 ** 63)));
assertTrue(compare_i64(true, Number.MIN_SAFE_INTEGER));
assertTrue(compare_i64(true, Number.MAX_SAFE_INTEGER));
assertThrows(() => compare_i64(false, -(2 ** 64)));
assertThrows(() => compare_i64(false, -(2 ** 64 + 1)));
assertThrows(() => compare_i64(false, 2 ** 64));
assertThrows(() => compare_i64(false, 2 ** 64 + 3.15));

// ----------- u64 -----------
function compare_u64(in_range, arg) {
  return fast_c_api.enforce_range_compare_u64(in_range, arg, arg);
}

%PrepareFunctionForOptimization(compare_u64);
assertFalse(compare_u64(true, 123));
%OptimizeFunctionOnNextCall(compare_u64);
assertTrue(compare_u64(true, 123));
assertTrue(compare_u64(true, 0));
assertTrue(compare_u64(true, -0.5));
assertTrue(compare_u64(true, 0.5));
assertTrue(compare_u64(true, 2 ** 32 - 1));
assertTrue(compare_u64(true, Number.MAX_SAFE_INTEGER));
assertThrows(() => compare_u64(false, 2 ** 64));
assertThrows(() => compare_u64(false, -1));
assertThrows(() => compare_u64(false, -1.5));
assertThrows(() => compare_u64(false, Number.MIN_SAFE_INTEGER));
assertThrows(() => compare_u64(false, 2 ** 64 + 3.15));

// ----------- clamp_compare -----------
// `clamp_compare` has the following signature:
// void clamp_compare(bool /*in_range*/,
//   double, integer_type)
// where integer_type = {int32_t, uint32_t, int64_t, uint64_t}

// ----------- i32 -----------
function is_in_range_i32(in_range, arg, expected) {
  let result = fast_c_api.clamp_compare_i32(in_range, arg, arg);

  assertEquals(expected, result);
}

%PrepareFunctionForOptimization(is_in_range_i32);
is_in_range_i32(true, 123, 123);
%OptimizeFunctionOnNextCall(is_in_range_i32);
is_in_range_i32(true, 123, 123);
is_in_range_i32(true, -0.5, 0);
is_in_range_i32(true, 0.5, 0);
is_in_range_i32(true, 1.5, 2);
is_in_range_i32(true, min_int32, min_int32);
is_in_range_i32(true, max_int32, max_int32);
// Slow path doesn't perform clamping.
if (isOptimized(is_in_range_i32)) {
  is_in_range_i32(false, -(2 ** 32), min_int32);
  is_in_range_i32(false, -(2 ** 32 + 1), min_int32);
  is_in_range_i32(false, 2 ** 32, max_int32);
  is_in_range_i32(false, 2 ** 32 + 3.15, max_int32);
  is_in_range_i32(false, Number.MIN_SAFE_INTEGER, min_int32);
  is_in_range_i32(false, Number.MAX_SAFE_INTEGER, max_int32);
}

// ----------- u32 -----------
function is_in_range_u32(in_range, arg, expected) {
  let result = fast_c_api.clamp_compare_u32(in_range, arg, arg);

  assertEquals(expected, result);
}

%PrepareFunctionForOptimization(is_in_range_u32);
is_in_range_u32(true, 123, 123);
%OptimizeFunctionOnNextCall(is_in_range_u32);
is_in_range_u32(true, 123, 123);
is_in_range_u32(true, 0, 0);
is_in_range_u32(true, -0.5, 0);
is_in_range_u32(true, 0.5, 0);
is_in_range_u32(true, 2 ** 32 - 1, max_uint32);
is_in_range_u32(false, -(2 ** 31), min_uint32);
is_in_range_u32(false, 2 ** 32, max_uint32);
is_in_range_u32(false, -1, min_uint32);
is_in_range_u32(false, -1.5, min_uint32);
is_in_range_u32(false, Number.MIN_SAFE_INTEGER, min_uint32);
is_in_range_u32(false, Number.MAX_SAFE_INTEGER, max_uint32);

// ----------- i64 -----------
function is_in_range_i64(in_range, arg, expected) {
  let result = fast_c_api.clamp_compare_i64(in_range, arg, arg);
  assertEquals(expected, result);
}

%PrepareFunctionForOptimization(is_in_range_i64);
is_in_range_i64(true, 123, 123);
%OptimizeFunctionOnNextCall(is_in_range_i64);
is_in_range_i64(true, 123, 123);
is_in_range_i64(true, -0.5, 0);
is_in_range_i64(true, 0.5, 0);
is_in_range_i64(true, 1.5, 2);
is_in_range_i64(true, Number.MIN_SAFE_INTEGER, Number.MIN_SAFE_INTEGER);
is_in_range_i64(true, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER);
is_in_range_i64(false, -(2 ** 63), Number.MIN_SAFE_INTEGER);
is_in_range_i64(false, 2 ** 63 - 1024, Number.MAX_SAFE_INTEGER);
is_in_range_i64(false, 2 ** 63, Number.MAX_SAFE_INTEGER);
is_in_range_i64(false, -(2 ** 64), Number.MIN_SAFE_INTEGER);
is_in_range_i64(false, -(2 ** 64 + 1), Number.MIN_SAFE_INTEGER);
is_in_range_i64(false, 2 ** 64, Number.MAX_SAFE_INTEGER);
is_in_range_i64(false, 2 ** 64 + 3.15, Number.MAX_SAFE_INTEGER);

// ----------- u64 -----------
function is_in_range_u64(in_range, arg, expected) {
  let result = fast_c_api.clamp_compare_u64(in_range, arg, arg);
  assertEquals(expected, result);
}

%PrepareFunctionForOptimization(is_in_range_u64);
is_in_range_u64(true, 123, 123);
%OptimizeFunctionOnNextCall(is_in_range_u64);
is_in_range_u64(true, 123, 123);
is_in_range_u64(true, 0, 0);
is_in_range_u64(true, -0.5, 0);
is_in_range_u64(true, 0.5, 0);
is_in_range_u64(true, 2 ** 32 - 1, 2 ** 32 - 1);
is_in_range_u64(true, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER);
is_in_range_u64(false, Number.MIN_SAFE_INTEGER, 0);
is_in_range_u64(false, -1, 0);
is_in_range_u64(false, -1.5, 0);
is_in_range_u64(false, 2 ** 64, Number.MAX_SAFE_INTEGER);
is_in_range_u64(false, 2 ** 64 + 3.15, Number.MAX_SAFE_INTEGER);

// ---------- invalid arguments for clamp_compare ---------
fast_c_api.clamp_compare_i32(true);
fast_c_api.clamp_compare_i32(true, 753801, -2147483650);
