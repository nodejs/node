// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file excercises sequences support for  fast API calls.

// Flags: --turbo-fast-api-calls --allow-natives-syntax --opt
// --always-opt is disabled because we rely on particular feedback for
// optimizing to the fastest path.
// Flags: --no-always-opt
// The test relies on optimizing/deoptimizing at predictable moments, so
// it's not suitable for deoptimization fuzzing.
// Flags: --deopt-every-n-times=0

d8.file.execute('test/mjsunit/compiler/fast-api-helpers.js');

const fast_c_api = new d8.test.FastCAPI();

// ----------- add_all_sequence -----------
// `add_all_sequence` has the following signature:
// double add_all_sequence(bool /*should_fallback*/, Local<Array>)

// Smi only test - regular call hits the fast path.
(function () {
  function add_all_sequence() {
    const arr = [-42, 45];
    return fast_c_api.add_all_sequence(false /* should_fallback */, arr);
  }
  ExpectFastCall(add_all_sequence, 3);
})();

(function () {
  function add_all_sequence_mismatch(arg) {
    return fast_c_api.add_all_sequence(false /*should_fallback*/, arg);
  }

  %PrepareFunctionForOptimization(add_all_sequence_mismatch);
  add_all_sequence_mismatch();
  %OptimizeFunctionOnNextCall(add_all_sequence_mismatch);

  // Test that passing non-array arguments falls down the slow path.
  assert_throws_and_optimized(add_all_sequence_mismatch, 42);
  assert_throws_and_optimized(add_all_sequence_mismatch, {});
  assert_throws_and_optimized(add_all_sequence_mismatch, 'string');
  assert_throws_and_optimized(add_all_sequence_mismatch, Symbol());
})();

//----------- Test function overloads with same arity. -----------
//Only overloads between JSArray and TypedArray are supported

// Test with TypedArray.
(function () {
  function overloaded_test(should_fallback = false) {
    let typed_array = new Uint32Array([1,2,3]);
    return fast_c_api.add_all_overload(false /* should_fallback */,
        typed_array);
  }
  ExpectFastCall(overloaded_test, 6);
})();

let large_array = [];
for (let i = 0; i < 100; i++) {
  large_array.push(i);
}

// Non-externalized TypedArray.
(function () {
  function overloaded_test(should_fallback = false) {
    let typed_array = new Uint32Array(large_array);
    return fast_c_api.add_all_overload(false /* should_fallback */,
      typed_array);
  }
  ExpectFastCall(overloaded_test, 4950);
})();

// Mismatched TypedArray.
(function () {
  function overloaded_test(should_fallback = false) {
    let typed_array = new Float32Array([1.1, 2.2, 3.3]);
    return fast_c_api.add_all_overload(false /* should_fallback */,
        typed_array);
  }
  ExpectSlowCall(overloaded_test, 6.6);
})();

// Test with JSArray.
(function () {
  function overloaded_test(should_fallback = false) {
    let js_array = [26, -6, 42];
    return fast_c_api.add_all_overload(false /* should_fallback */, js_array);
  }
  ExpectFastCall(overloaded_test, 62);
})();

// Test function overloads with undefined.
(function () {
  function overloaded_test(should_fallback = false) {
    return fast_c_api.add_all_overload(false /* should_fallback */, undefined);
  }
  ExpectSlowCall(overloaded_test, 0);
})();

// Test function with invalid overloads.
(function () {
  function overloaded_test(should_fallback = false) {
    return fast_c_api.add_all_invalid_overload(false /* should_fallback */,
      [26, -6, 42]);
  }

  %PrepareFunctionForOptimization(overloaded_test);
  result = overloaded_test();
  assertEquals(62, result);

  fast_c_api.reset_counts();
  %OptimizeFunctionOnNextCall(overloaded_test);
  result = overloaded_test();
  assertEquals(62, result);
  // Here we deopt because with this invalid overload:
  // - add_all_int_invalid_func(Receiver, Bool, Int32, Options)
  // - add_all_seq_c_func(Receiver, Bool, JSArray, Options)
  // we expect that a number will be passed as 3rd argument
  // (SimplifiedLowering takes the type from the first overloaded function).
  assertUnoptimized(overloaded_test);
  assertEquals(0, fast_c_api.fast_call_count());
})();

//----------- Test different TypedArray functions. -----------
// ----------- add_all_<TYPE>_typed_array -----------
// `add_all_<TYPE>_typed_array` have the following signature:
// double add_all_<TYPE>_typed_array(bool /*should_fallback*/, FastApiTypedArray<TYPE>)

(function () {
  function int32_test(should_fallback = false) {
    let typed_array = new Int32Array([-42, 1, 2, 3]);
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectFastCall(int32_test, -36);
})();

(function () {
  function uint32_test(should_fallback = false) {
    let typed_array = new Uint32Array([1, 2, 3]);
    return fast_c_api.add_all_uint32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectFastCall(uint32_test, 6);
})();

(function () {
  function detached_typed_array_test(should_fallback = false) {
    let typed_array = new Int32Array([-42, 1, 2, 3]);
    %ArrayBufferDetach(typed_array.buffer);
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectSlowCall(detached_typed_array_test, 0);
})();

(function () {
  function detached_non_ext_typed_array_test(should_fallback = false) {
    let typed_array = new Int32Array(large_array);
    %ArrayBufferDetach(typed_array.buffer);
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectSlowCall(detached_non_ext_typed_array_test, 0);
})();

(function () {
  function shared_array_buffer_ta_test(should_fallback = false) {
    let sab = new SharedArrayBuffer(16);
    let typed_array = new Int32Array(sab);
    typed_array.set([-42, 1, 2, 3]);
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectSlowCall(shared_array_buffer_ta_test, -36);
})();

(function () {
  function shared_array_buffer_ext_ta_test(should_fallback = false) {
    let sab = new SharedArrayBuffer(400);
    let typed_array = new Int32Array(sab);
    typed_array.set(large_array);
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectSlowCall(shared_array_buffer_ext_ta_test, 4950);
})();

// Empty TypedArray.
(function () {
  function int32_test(should_fallback = false) {
    let typed_array = new Int32Array(0);
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      typed_array);
  }
  ExpectFastCall(int32_test, 0);
})();

// Invalid argument types instead of a TypedArray.
(function () {
  function invalid_test(arg) {
    return fast_c_api.add_all_int32_typed_array(false /* should_fallback */,
      arg);
  }
  %PrepareFunctionForOptimization(invalid_test);
  invalid_test(new Int32Array(0));
  %OptimizeFunctionOnNextCall(invalid_test);

  assert_throws_and_optimized(invalid_test, 42);
  assert_throws_and_optimized(invalid_test, {});
  assert_throws_and_optimized(invalid_test, 'string');
  assert_throws_and_optimized(invalid_test, Symbol());
})();
