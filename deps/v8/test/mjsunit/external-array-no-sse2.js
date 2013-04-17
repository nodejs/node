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

// Flags: --allow-natives-syntax --expose-gc --noenable-sse2

// Helper
function assertInstance(o, f) {
  assertSame(o.constructor, f);
  assertInstanceof(o, f);
}

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
assertInstance(ab, ArrayBuffer);
var derived_uint8 = new Uint8Array(ab);
assertInstance(derived_uint8, Uint8Array);
assertSame(ab, derived_uint8.buffer);
assertEquals(12, derived_uint8.length);
assertEquals(12, derived_uint8.byteLength);
assertEquals(0, derived_uint8.byteOffset);
assertEquals(1, derived_uint8.BYTES_PER_ELEMENT);
var derived_uint8_2 = new Uint8Array(ab,7);
assertInstance(derived_uint8_2, Uint8Array);
assertSame(ab, derived_uint8_2.buffer);
assertEquals(5, derived_uint8_2.length);
assertEquals(5, derived_uint8_2.byteLength);
assertEquals(7, derived_uint8_2.byteOffset);
assertEquals(1, derived_uint8_2.BYTES_PER_ELEMENT);
var derived_int16 = new Int16Array(ab);
assertInstance(derived_int16, Int16Array);
assertSame(ab, derived_int16.buffer);
assertEquals(6, derived_int16.length);
assertEquals(12, derived_int16.byteLength);
assertEquals(0, derived_int16.byteOffset);
assertEquals(2, derived_int16.BYTES_PER_ELEMENT);
var derived_int16_2 = new Int16Array(ab,6);
assertInstance(derived_int16_2, Int16Array);
assertSame(ab, derived_int16_2.buffer);
assertEquals(3, derived_int16_2.length);
assertEquals(6, derived_int16_2.byteLength);
assertEquals(6, derived_int16_2.byteOffset);
assertEquals(2, derived_int16_2.BYTES_PER_ELEMENT);
var derived_uint32 = new Uint32Array(ab);
assertInstance(derived_uint32, Uint32Array);
assertSame(ab, derived_uint32.buffer);
assertEquals(3, derived_uint32.length);
assertEquals(12, derived_uint32.byteLength);
assertEquals(0, derived_uint32.byteOffset);
assertEquals(4, derived_uint32.BYTES_PER_ELEMENT);
var derived_uint32_2 = new Uint32Array(ab,4);
assertInstance(derived_uint32_2, Uint32Array);
assertSame(ab, derived_uint32_2.buffer);
assertEquals(2, derived_uint32_2.length);
assertEquals(8, derived_uint32_2.byteLength);
assertEquals(4, derived_uint32_2.byteOffset);
assertEquals(4, derived_uint32_2.BYTES_PER_ELEMENT);
var derived_uint32_3 = new Uint32Array(ab,4,1);
assertInstance(derived_uint32_3, Uint32Array);
assertSame(ab, derived_uint32_3.buffer);
assertEquals(1, derived_uint32_3.length);
assertEquals(4, derived_uint32_3.byteLength);
assertEquals(4, derived_uint32_3.byteOffset);
assertEquals(4, derived_uint32_3.BYTES_PER_ELEMENT);
var derived_float64 = new Float64Array(ab,0,1);
assertInstance(derived_float64, Float64Array);
assertSame(ab, derived_float64.buffer);
assertEquals(1, derived_float64.length);
assertEquals(8, derived_float64.byteLength);
assertEquals(0, derived_float64.byteOffset);
assertEquals(8, derived_float64.BYTES_PER_ELEMENT);

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

// Test that an array constructed without an array buffer creates one properly.
a = new Uint8Array(31);
assertEquals(a.byteLength, a.buffer.byteLength);
assertEquals(a.length, a.buffer.byteLength);
assertEquals(a.length * a.BYTES_PER_ELEMENT, a.buffer.byteLength);
a = new Int16Array(5);
assertEquals(a.byteLength, a.buffer.byteLength);
assertEquals(a.length * a.BYTES_PER_ELEMENT, a.buffer.byteLength);
a = new Float64Array(7);
assertEquals(a.byteLength, a.buffer.byteLength);
assertEquals(a.length * a.BYTES_PER_ELEMENT, a.buffer.byteLength);

// Test that an implicitly created buffer is a valid buffer.
a = new Float64Array(7);
assertSame(a.buffer, (new Uint16Array(a.buffer)).buffer);
assertSame(a.buffer, (new Float32Array(a.buffer,4)).buffer);
assertSame(a.buffer, (new Int8Array(a.buffer,3,51)).buffer);
assertInstance(a.buffer, ArrayBuffer);

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
         Uint32Array, Uint8ClampedArray, Float32Array, Float64Array];

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


// Check handling of 0-sized buffers and arrays.
ab = new ArrayBuffer(0);
assertInstance(ab, ArrayBuffer);
assertEquals(0, ab.byteLength);
a = new Int8Array(ab);
assertInstance(a, Int8Array);
assertEquals(0, a.byteLength);
assertEquals(0, a.length);
a[0] = 1;
assertEquals(undefined, a[0]);
ab = new ArrayBuffer(16);
assertInstance(ab, ArrayBuffer);
a = new Float32Array(ab,4,0);
assertInstance(a, Float32Array);
assertEquals(0, a.byteLength);
assertEquals(0, a.length);
a[0] = 1;
assertEquals(undefined, a[0]);
a = new Uint16Array(0);
assertInstance(a, Uint16Array);
assertEquals(0, a.byteLength);
assertEquals(0, a.length);
a[0] = 1;
assertEquals(undefined, a[0]);


// Check construction from arrays.
a = new Uint32Array([]);
assertInstance(a, Uint32Array);
assertEquals(0, a.length);
assertEquals(0, a.byteLength);
assertEquals(0, a.buffer.byteLength);
assertEquals(4, a.BYTES_PER_ELEMENT);
assertInstance(a.buffer, ArrayBuffer);
a = new Uint16Array([1,2,3]);
assertInstance(a, Uint16Array);
assertEquals(3, a.length);
assertEquals(6, a.byteLength);
assertEquals(6, a.buffer.byteLength);
assertEquals(2, a.BYTES_PER_ELEMENT);
assertEquals(1, a[0]);
assertEquals(3, a[2]);
assertInstance(a.buffer, ArrayBuffer);
a = new Uint32Array(a);
assertInstance(a, Uint32Array);
assertEquals(3, a.length);
assertEquals(12, a.byteLength);
assertEquals(12, a.buffer.byteLength);
assertEquals(4, a.BYTES_PER_ELEMENT);
assertEquals(1, a[0]);
assertEquals(3, a[2]);
assertInstance(a.buffer, ArrayBuffer);

// Check subarrays.
a = new Uint16Array([1,2,3,4,5,6]);
aa = a.subarray(3);
assertInstance(aa, Uint16Array);
assertEquals(3, aa.length);
assertEquals(6, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(3,5);
assertInstance(aa, Uint16Array);
assertEquals(2, aa.length);
assertEquals(4, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(4,8);
assertInstance(aa, Uint16Array);
assertEquals(2, aa.length);
assertEquals(4, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(9);
assertInstance(aa, Uint16Array);
assertEquals(0, aa.length);
assertEquals(0, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(-4);
assertInstance(aa, Uint16Array);
assertEquals(4, aa.length);
assertEquals(8, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(-3,-1);
assertInstance(aa, Uint16Array);
assertEquals(2, aa.length);
assertEquals(4, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(3,2);
assertInstance(aa, Uint16Array);
assertEquals(0, aa.length);
assertEquals(0, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(-3,-4);
assertInstance(aa, Uint16Array);
assertEquals(0, aa.length);
assertEquals(0, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);
aa = a.subarray(0,-8);
assertInstance(aa, Uint16Array);
assertEquals(0, aa.length);
assertEquals(0, aa.byteLength);
assertEquals(2, aa.BYTES_PER_ELEMENT);
assertSame(a.buffer, aa.buffer);

assertThrows(function(){ a.subarray.call({}, 0) });
assertThrows(function(){ a.subarray.call([], 0) });
assertThrows(function(){ a.subarray.call(a) });


// Call constructors directly as functions, and through .call and .apply

b = ArrayBuffer(100)
a = Int8Array(b, 5, 77)
assertInstance(b, ArrayBuffer)
assertInstance(a, Int8Array)
assertSame(b, a.buffer)
assertEquals(5, a.byteOffset)
assertEquals(77, a.byteLength)
b = ArrayBuffer.call(null, 10)
a = Uint16Array.call(null, b, 2, 4)
assertInstance(b, ArrayBuffer)
assertInstance(a, Uint16Array)
assertSame(b, a.buffer)
assertEquals(2, a.byteOffset)
assertEquals(8, a.byteLength)
b = ArrayBuffer.apply(null, [1000])
a = Float32Array.apply(null, [b, 128, 1])
assertInstance(b, ArrayBuffer)
assertInstance(a, Float32Array)
assertSame(b, a.buffer)
assertEquals(128, a.byteOffset)
assertEquals(4, a.byteLength)


// Test array.set in different combinations.

function assertArrayPrefix(expected, array) {
  for (var i = 0; i < expected.length; ++i) {
    assertEquals(expected[i], array[i]);
  }
}

var a11 = new Int16Array([1, 2, 3, 4, 0, -1])
var a12 = new Uint16Array(15)
a12.set(a11, 3)
assertArrayPrefix([0, 0, 0, 1, 2, 3, 4, 0, 0xffff, 0, 0], a12)
assertThrows(function(){ a11.set(a12) })

var a21 = [1, undefined, 10, NaN, 0, -1, {valueOf: function() {return 3}}]
var a22 = new Int32Array(12)
a22.set(a21, 2)
assertArrayPrefix([0, 0, 1, 0, 10, 0, 0, -1, 3, 0], a22)

var a31 = new Float32Array([2, 4, 6, 8, 11, NaN, 1/0, -3])
var a32 = a31.subarray(2, 6)
a31.set(a32, 4)
assertArrayPrefix([2, 4, 6, 8, 6, 8, 11, NaN], a31)
assertArrayPrefix([6, 8, 6, 8], a32)

var a4 = new Uint8ClampedArray([3,2,5,6])
a4.set(a4)
assertArrayPrefix([3, 2, 5, 6], a4)

// Cases with overlapping backing store but different element sizes.
var b = new ArrayBuffer(4)
var a5 = new Int16Array(b)
var a50 = new Int8Array(b)
var a51 = new Int8Array(b, 0, 2)
var a52 = new Int8Array(b, 1, 2)
var a53 = new Int8Array(b, 2, 2)

a5.set([0x5050, 0x0a0a])
assertArrayPrefix([0x50, 0x50, 0x0a, 0x0a], a50)
assertArrayPrefix([0x50, 0x50], a51)
assertArrayPrefix([0x50, 0x0a], a52)
assertArrayPrefix([0x0a, 0x0a], a53)

a50.set([0x50, 0x50, 0x0a, 0x0a])
a51.set(a5)
assertArrayPrefix([0x50, 0x0a, 0x0a, 0x0a], a50)

a50.set([0x50, 0x50, 0x0a, 0x0a])
a52.set(a5)
assertArrayPrefix([0x50, 0x50, 0x0a, 0x0a], a50)

a50.set([0x50, 0x50, 0x0a, 0x0a])
a53.set(a5)
assertArrayPrefix([0x50, 0x50, 0x50, 0x0a], a50)

a50.set([0x50, 0x51, 0x0a, 0x0b])
a5.set(a51)
assertArrayPrefix([0x0050, 0x0051], a5)

a50.set([0x50, 0x51, 0x0a, 0x0b])
a5.set(a52)
assertArrayPrefix([0x0051, 0x000a], a5)

a50.set([0x50, 0x51, 0x0a, 0x0b])
a5.set(a53)
assertArrayPrefix([0x000a, 0x000b], a5)

// Mixed types of same size.
var a61 = new Float32Array([1.2, 12.3])
var a62 = new Int32Array(2)
a62.set(a61)
assertArrayPrefix([1, 12], a62)
a61.set(a62)
assertArrayPrefix([1, 12], a61)

// Invalid source
assertThrows(function() { a.set(0) })
assertThrows(function() { a.set({}) })


// Test arraybuffer.slice

var a0 = new Int8Array([1, 2, 3, 4, 5, 6])
var b0 = a0.buffer

var b1 = b0.slice(0)
assertEquals(b0.byteLength, b1.byteLength)
assertArrayPrefix([1, 2, 3, 4, 5, 6], Int8Array(b1))

var b2 = b0.slice(3)
assertEquals(b0.byteLength - 3, b2.byteLength)
assertArrayPrefix([4, 5, 6], Int8Array(b2))

var b3 = b0.slice(2, 4)
assertEquals(2, b3.byteLength)
assertArrayPrefix([3, 4], Int8Array(b3))

function goo(a, i) {
  return a[i];
}

function boo(a, i, v) {
  return a[i] = v;
}

function do_tagged_index_external_array_test(constructor) {
  var t_array = new constructor([1, 2, 3, 4, 5, 6]);
  assertEquals(1, goo(t_array, 0));
  assertEquals(1, goo(t_array, 0));
  boo(t_array, 0, 13);
  assertEquals(13, goo(t_array, 0));
  %OptimizeFunctionOnNextCall(goo);
  %OptimizeFunctionOnNextCall(boo);
  boo(t_array, 0, 15);
  assertEquals(15, goo(t_array, 0));
  %ClearFunctionTypeFeedback(goo);
  %ClearFunctionTypeFeedback(boo);
}

do_tagged_index_external_array_test(Int8Array);
do_tagged_index_external_array_test(Uint8Array);
do_tagged_index_external_array_test(Int16Array);
do_tagged_index_external_array_test(Uint16Array);
do_tagged_index_external_array_test(Int32Array);
do_tagged_index_external_array_test(Uint32Array);
do_tagged_index_external_array_test(Float32Array);
do_tagged_index_external_array_test(Float64Array);

var built_in_array = new Array(1, 2, 3, 4, 5, 6);
assertEquals(1, goo(built_in_array, 0));
assertEquals(1, goo(built_in_array, 0));
%OptimizeFunctionOnNextCall(goo);
%OptimizeFunctionOnNextCall(boo);
boo(built_in_array, 0, 11);
assertEquals(11, goo(built_in_array, 0));
%ClearFunctionTypeFeedback(goo);
%ClearFunctionTypeFeedback(boo);

built_in_array = new Array(1.5, 2, 3, 4, 5, 6);
assertEquals(1.5, goo(built_in_array, 0));
assertEquals(1.5, goo(built_in_array, 0));
%OptimizeFunctionOnNextCall(goo);
%OptimizeFunctionOnNextCall(boo);
boo(built_in_array, 0, 2.5);
assertEquals(2.5, goo(built_in_array, 0));
%ClearFunctionTypeFeedback(goo);
%ClearFunctionTypeFeedback(boo);
