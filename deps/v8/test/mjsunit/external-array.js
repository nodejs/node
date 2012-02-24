// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --expose-gc

// This is a regression test for overlapping key and value registers.
function f(a) {
  a[0] = 0;
  a[1] = 0;
}

var a = new Int32Array(2);
for (var i = 0; i < 5; i++) {
  f(a);
}
%OptimizeFunctionOnNextCall(f);
f(a);

assertEquals(0, a[0]);
assertEquals(0, a[1]);

// No-parameter constructor should fail right now.
function abfunc1() {
  return new ArrayBuffer();
}
assertThrows(abfunc1);

// Test derivation from an ArrayBuffer
var ab = new ArrayBuffer(12);
var derived_uint8 = new Uint8Array(ab);
assertEquals(12, derived_uint8.length);
var derived_uint32 = new Uint32Array(ab);
assertEquals(3, derived_uint32.length);
var derived_uint32_2 = new Uint32Array(ab,4);
assertEquals(2, derived_uint32_2.length);
var derived_uint32_3 = new Uint32Array(ab,4,1);
assertEquals(1, derived_uint32_3.length);

// If a given byteOffset and length references an area beyond the end of the
// ArrayBuffer an exception is raised.
function abfunc3() {
  new Uint32Array(ab,4,3);
}
assertThrows(abfunc3);
function abfunc4() {
  new Uint32Array(ab,16);
}
assertThrows(abfunc4);

// The given byteOffset must be a multiple of the element size of the specific
// type, otherwise an exception is raised.
function abfunc5() {
  new Uint32Array(ab,5);
}
assertThrows(abfunc5);

// If length is not explicitly specified, the length of the ArrayBuffer minus
// the byteOffset must be a multiple of the element size of the specific type,
// or an exception is raised.
var ab2 = new ArrayBuffer(13);
function abfunc6() {
  new Uint32Array(ab2,4);
}
assertThrows(abfunc6);

// Test the correct behavior of the |BYTES_PER_ELEMENT| property (which is
// "constant", but not read-only).
a = new Int32Array(2);
assertEquals(4, a.BYTES_PER_ELEMENT);
a.BYTES_PER_ELEMENT = 42;
assertEquals(42, a.BYTES_PER_ELEMENT);
a = new Uint8Array(2);
assertEquals(1, a.BYTES_PER_ELEMENT);
a = new Int16Array(2);
assertEquals(2, a.BYTES_PER_ELEMENT);

// Test Float64Arrays.
function get(a, index) {
  return a[index];
}
function set(a, index, value) {
  a[index] = value;
}
function temp() {
var array = new Float64Array(2);
for (var i = 0; i < 5; i++) {
  set(array, 0, 2.5);
  assertEquals(2.5, array[0]);
}
%OptimizeFunctionOnNextCall(set);
set(array, 0, 2.5);
assertEquals(2.5, array[0]);
set(array, 1, 3.5);
assertEquals(3.5, array[1]);
for (var i = 0; i < 5; i++) {
  assertEquals(2.5, get(array, 0));
  assertEquals(3.5, array[1]);
}
%OptimizeFunctionOnNextCall(get);
assertEquals(2.5, get(array, 0));
assertEquals(3.5, get(array, 1));
}

// Test non-number parameters.
var array_with_length_from_non_number = new Int32Array("2");
assertEquals(2, array_with_length_from_non_number.length);
array_with_length_from_non_number = new Int32Array(undefined);
assertEquals(0, array_with_length_from_non_number.length);
var foo = { valueOf: function() { return 3; } };
array_with_length_from_non_number = new Int32Array(foo);
assertEquals(3, array_with_length_from_non_number.length);
foo = { toString: function() { return "4"; } };
array_with_length_from_non_number = new Int32Array(foo);
assertEquals(4, array_with_length_from_non_number.length);


// Test loads and stores.
types = [Array, Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array,
         Uint32Array, PixelArray, Float32Array, Float64Array];

test_result_nan = [NaN, 0, 0, 0, 0, 0, 0, 0, NaN, NaN];
test_result_low_int = [-1, -1, 255, -1, 65535, -1, 0xFFFFFFFF, 0, -1, -1];
test_result_low_double = [-1.25, -1, 255, -1, 65535, -1, 0xFFFFFFFF, 0, -1.25, -1.25];
test_result_middle = [253.75, -3, 253, 253, 253, 253, 253, 254, 253.75, 253.75];
test_result_high_int = [256, 0, 0, 256, 256, 256, 256, 255, 256, 256];
test_result_high_double = [256.25, 0, 0, 256, 256, 256, 256, 255, 256.25, 256.25];

const kElementCount = 40;

function test_load(array, sum) {
  for (var i = 0; i < kElementCount; i++) {
    sum += array[i];
  }
  return sum;
}

function test_load_const_key(array, sum) {
  sum += array[0];
  sum += array[1];
  sum += array[2];
  return sum;
}

function test_store(array, sum) {
  for (var i = 0; i < kElementCount; i++) {
    sum += array[i] = i+1;
  }
  return sum;
}

function test_store_const_key(array, sum) {
  sum += array[0] = 1;
  sum += array[1] = 2;
  sum += array[2] = 3;
  return sum;
}

function zero() {
  return 0.0;
}

function test_store_middle_tagged(array, sum) {
  array[0] = 253.75;
  return array[0];
}

function test_store_high_tagged(array, sum) {
  array[0] = 256.25;
  return array[0];
}

function test_store_middle_double(array, sum) {
  array[0] = 253.75 + zero(); // + forces double type feedback
  return array[0];
}

function test_store_high_double(array, sum) {
  array[0] = 256.25 + zero(); // + forces double type feedback
  return array[0];
}

function test_store_high_double(array, sum) {
  array[0] = 256.25;
  return array[0];
}

function test_store_low_int(array, sum) {
  array[0] = -1;
  return array[0];
}

function test_store_low_tagged(array, sum) {
  array[0] = -1.25;
  return array[0];
}

function test_store_low_double(array, sum) {
  array[0] = -1.25 + zero(); // + forces double type feedback
  return array[0];
}

function test_store_high_int(array, sum) {
  array[0] = 256;
  return array[0];
}

function test_store_nan(array, sum) {
  array[0] = NaN;
  return array[0];
}

const kRuns = 10;

function run_test(test_func, array, expected_result) {
  for (var i = 0; i < 5; i++) test_func(array, 0);
  %OptimizeFunctionOnNextCall(test_func);
  var sum = 0;
  for (var i = 0; i < kRuns; i++) {
    sum = test_func(array, sum);
  }
  assertEquals(expected_result, sum);
  %DeoptimizeFunction(test_func);
  gc();  // Makes V8 forget about type information for test_func.
}

function run_bounds_test(test_func, array, expected_result) {
  assertEquals(undefined, a[kElementCount]);
  a[kElementCount] = 456;
  assertEquals(undefined, a[kElementCount]);
  assertEquals(undefined, a[kElementCount+1]);
  a[kElementCount+1] = 456;
  assertEquals(undefined, a[kElementCount+1]);
}

for (var t = 0; t < types.length; t++) {
  var type = types[t];
  var a = new type(kElementCount);

  for (var i = 0; i < kElementCount; i++) {
    a[i] = i;
  }

  // Run test functions defined above.
  run_test(test_load, a, 780 * kRuns);
  run_test(test_load_const_key, a, 3 * kRuns);
  run_test(test_store, a, 820 * kRuns);
  run_test(test_store_const_key, a, 6 * kRuns);
  run_test(test_store_low_int, a, test_result_low_int[t]);
  run_test(test_store_low_double, a, test_result_low_double[t]);
  run_test(test_store_low_tagged, a, test_result_low_double[t]);
  run_test(test_store_high_int, a, test_result_high_int[t]);
  run_test(test_store_nan, a, test_result_nan[t]);
  run_test(test_store_middle_double, a, test_result_middle[t]);
  run_test(test_store_middle_tagged, a, test_result_middle[t]);
  run_test(test_store_high_double, a, test_result_high_double[t]);
  run_test(test_store_high_tagged, a, test_result_high_double[t]);

  // Test the correct behavior of the |length| property (which is read-only).
  if (t != 0) {
    assertEquals(kElementCount, a.length);
    a.length = 2;
    assertEquals(kElementCount, a.length);
    assertTrue(delete a.length);
    a.length = 2;
    assertEquals(2, a.length);

    // Make sure bounds checks are handled correctly for external arrays.
    run_bounds_test(a);
    run_bounds_test(a);
    run_bounds_test(a);
    %OptimizeFunctionOnNextCall(run_bounds_test);
    run_bounds_test(a);
    %DeoptimizeFunction(run_bounds_test);
    gc();  // Makes V8 forget about type information for test_func.

  }

  function array_load_set_smi_check(a) {
    return a[0] = a[0] = 1;
  }

  array_load_set_smi_check(a);
  array_load_set_smi_check(0);

  function array_load_set_smi_check2(a) {
    return a[0] = a[0] = 1;
  }

  array_load_set_smi_check2(a);
  %OptimizeFunctionOnNextCall(array_load_set_smi_check2);
  array_load_set_smi_check2(a);
  array_load_set_smi_check2(0);
  %DeoptimizeFunction(array_load_set_smi_check2);
  gc();  // Makes V8 forget about type information for array_load_set_smi_check.
}

// Check handling of undefined in 32- and 64-bit external float arrays.

function store_float32_undefined(ext_array) {
  ext_array[0] = undefined;
}

var float32_array = new Float32Array(1);
// Make sure runtime does it right
store_float32_undefined(float32_array);
assertTrue(isNaN(float32_array[0]));
// Make sure the ICs do it right
store_float32_undefined(float32_array);
assertTrue(isNaN(float32_array[0]));
// Make sure that Cranskshft does it right.
%OptimizeFunctionOnNextCall(store_float32_undefined);
store_float32_undefined(float32_array);
assertTrue(isNaN(float32_array[0]));

function store_float64_undefined(ext_array) {
  ext_array[0] = undefined;
}

var float64_array = new Float64Array(1);
// Make sure runtime does it right
store_float64_undefined(float64_array);
assertTrue(isNaN(float64_array[0]));
// Make sure the ICs do it right
store_float64_undefined(float64_array);
assertTrue(isNaN(float64_array[0]));
// Make sure that Cranskshft does it right.
%OptimizeFunctionOnNextCall(store_float64_undefined);
store_float64_undefined(float64_array);
assertTrue(isNaN(float64_array[0]));
