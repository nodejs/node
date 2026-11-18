// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0

// 1. TypedArray length and indexed element access KNA CSE across non-eagerly
// inlined helpers in pure regions and effect-free loops.
function readTypedArrayElem(ta, i) {
  return ta[i] + ta.length;
}

function writeTypedArrayElem(ta, i, v) {
  ta[i] = v;
  return ta.length;
}

function testTypedArrayKnaCse(ta, i) {
  let a = readTypedArrayElem(ta, i);
  let len = writeTypedArrayElem(ta, i + 1, a);
  let b = 0;
  for (let k = 0; k < 2; k++) {
    b += readTypedArrayElem(ta, i + 1);
  }
  return a + b + len + ta.length;
}

const ta = new Int32Array([10, 20, 30, 40]);
%PrepareFunctionForOptimization(readTypedArrayElem);
%PrepareFunctionForOptimization(writeTypedArrayElem);
%PrepareFunctionForOptimization(testTypedArrayKnaCse);
assertEquals(58, testTypedArrayKnaCse(ta, 0));
assertEquals(70, testTypedArrayKnaCse(ta, 1));
%OptimizeMaglevOnNextCall(testTypedArrayKnaCse);
ta[0] = 10;
ta[1] = 20;
ta[2] = 30;
ta[3] = 40;
assertEquals(58, testTypedArrayKnaCse(ta, 0));
assertEquals(70, testTypedArrayKnaCse(ta, 1));
assertOptimized(testTypedArrayKnaCse);
%OptimizeFunctionOnNextCall(testTypedArrayKnaCse);
ta[0] = 10;
ta[1] = 20;
ta[2] = 30;
ta[3] = 40;
assertEquals(58, testTypedArrayKnaCse(ta, 0));
assertEquals(70, testTypedArrayKnaCse(ta, 1));
assertOptimized(testTypedArrayKnaCse);

// 2. DataView byteLength and data pointer KNA CSE across non-eagerly inlined
// helpers in pure regions and effect-free loops.
function readDataViewInt32(dv, offset) {
  return dv.getInt32(offset, true) + dv.byteLength;
}

function writeDataViewInt32(dv, offset, val) {
  dv.setInt32(offset, val, true);
  return dv.byteLength;
}

function readDataViewFloat64(dv, offset) {
  return dv.getFloat64(offset, true);
}

function testDataViewKnaCse(dv, offset) {
  let v0 = readDataViewInt32(dv, offset);
  let len = writeDataViewInt32(dv, offset + 4, v0);
  let v1 = 0;
  for (let k = 0; k < 2; k++) {
    v1 += readDataViewInt32(dv, offset + 4);
  }
  let f = readDataViewFloat64(dv, offset + 8);
  return v0 + v1 + len + dv.byteLength + f;
}

const ab = new ArrayBuffer(16);
const dv = new DataView(ab);
dv.setInt32(0, 7, true);
dv.setInt32(4, 11, true);
dv.setFloat64(8, 2.5, true);

%PrepareFunctionForOptimization(readDataViewInt32);
%PrepareFunctionForOptimization(writeDataViewInt32);
%PrepareFunctionForOptimization(readDataViewFloat64);
%PrepareFunctionForOptimization(testDataViewKnaCse);
assertEquals(135.5, testDataViewKnaCse(dv, 0));
assertEquals(135.5, testDataViewKnaCse(dv, 0));
%OptimizeMaglevOnNextCall(testDataViewKnaCse);
assertEquals(135.5, testDataViewKnaCse(dv, 0));
assertOptimized(testDataViewKnaCse);
%OptimizeFunctionOnNextCall(testDataViewKnaCse);
assertEquals(135.5, testDataViewKnaCse(dv, 0));
assertOptimized(testDataViewKnaCse);

// 3. Verify that detaching a single-view TypedArray across a side-effecting
// call invalidates the cached length and returns 0 after the call without
// relying on ArrayBufferDetachingProtector.
let should_detach = false;
function maybeDetach(buf) {
  if (should_detach) {
    %ArrayBufferDetach(buf);
  }
}
%NeverOptimizeFunction(maybeDetach);

function testTypedArrayDetachInvalidation(arr, buf) {
  let before = readTypedArrayElem(arr, 0);
  maybeDetach(buf);
  return before + arr.length;
}

const ab_ta1 = new ArrayBuffer(16);
const ta_detach1 = new Int32Array(ab_ta1);
ta_detach1[0] = 5;
%PrepareFunctionForOptimization(testTypedArrayDetachInvalidation);
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach1, ab_ta1));
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach1, ab_ta1));
%OptimizeMaglevOnNextCall(testTypedArrayDetachInvalidation);
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach1, ab_ta1));
assertOptimized(testTypedArrayDetachInvalidation);
should_detach = true;
assertEquals(9, testTypedArrayDetachInvalidation(ta_detach1, ab_ta1));

// Re-optimize in Maglev after the first detach (Int32Array map is now unstable
// and ArrayBufferDetachingProtector is still intact under
// --track-array-buffer-views). First test a function with monomorphic
// non-detached feedback while Int32Array's map is already unstable:
function testTypedArrayDetachInvalidationUnstableMap(arr, buf) {
  let before = readTypedArrayElem(arr, 0);
  maybeDetach(buf);
  return before + arr.length;
}

should_detach = false;
const ab_ta2 = new ArrayBuffer(16);
const ta_detach2 = new Int32Array(ab_ta2);
ta_detach2[0] = 5;
%PrepareFunctionForOptimization(testTypedArrayDetachInvalidationUnstableMap);
assertEquals(
    13, testTypedArrayDetachInvalidationUnstableMap(ta_detach2, ab_ta2));
assertEquals(
    13, testTypedArrayDetachInvalidationUnstableMap(ta_detach2, ab_ta2));
%OptimizeMaglevOnNextCall(testTypedArrayDetachInvalidationUnstableMap);
assertEquals(
    13, testTypedArrayDetachInvalidationUnstableMap(ta_detach2, ab_ta2));
assertOptimized(testTypedArrayDetachInvalidationUnstableMap);
should_detach = true;
assertEquals(
    9, testTypedArrayDetachInvalidationUnstableMap(ta_detach2, ab_ta2));

// Also re-optimize testTypedArrayDetachInvalidation (which now has polymorphic
// [Int32Array, DetachedInt32Array] feedback) in Maglev and then top-tier:
should_detach = false;
const ab_ta3 = new ArrayBuffer(16);
const ta_detach3 = new Int32Array(ab_ta3);
ta_detach3[0] = 5;
%PrepareFunctionForOptimization(testTypedArrayDetachInvalidation);
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach3, ab_ta3));
%OptimizeMaglevOnNextCall(testTypedArrayDetachInvalidation);
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach3, ab_ta3));
assertOptimized(testTypedArrayDetachInvalidation);
should_detach = true;
assertEquals(9, testTypedArrayDetachInvalidation(ta_detach3, ab_ta3));

should_detach = false;
const ab_ta4 = new ArrayBuffer(16);
const ta_detach4 = new Int32Array(ab_ta4);
ta_detach4[0] = 5;
%PrepareFunctionForOptimization(testTypedArrayDetachInvalidation);
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach4, ab_ta4));
%OptimizeFunctionOnNextCall(testTypedArrayDetachInvalidation);
assertEquals(13, testTypedArrayDetachInvalidation(ta_detach4, ab_ta4));
assertOptimized(testTypedArrayDetachInvalidation);
should_detach = true;
assertEquals(9, testTypedArrayDetachInvalidation(ta_detach4, ab_ta4));

// 4. Verify that detaching a DataView's underlying ArrayBuffer across a
// side-effecting call invalidates cached DataView byteLength / data pointer.
should_detach = false;
function testDataViewDetachInvalidation(view, buf) {
  let before = readDataViewInt32(view, 0);
  maybeDetach(buf);
  return before + readDataViewInt32(view, 0);
}

const ab_dv1 = new ArrayBuffer(16);
const dv_detach1 = new DataView(ab_dv1);
dv_detach1.setInt32(0, 3, true);
%PrepareFunctionForOptimization(testDataViewDetachInvalidation);
assertEquals(38, testDataViewDetachInvalidation(dv_detach1, ab_dv1));
assertEquals(38, testDataViewDetachInvalidation(dv_detach1, ab_dv1));
%OptimizeMaglevOnNextCall(testDataViewDetachInvalidation);
assertEquals(38, testDataViewDetachInvalidation(dv_detach1, ab_dv1));
assertOptimized(testDataViewDetachInvalidation);
should_detach = true;
assertThrows(
    () => testDataViewDetachInvalidation(dv_detach1, ab_dv1), TypeError);

should_detach = false;
const ab_dv2 = new ArrayBuffer(16);
const dv_detach2 = new DataView(ab_dv2);
dv_detach2.setInt32(0, 3, true);
%PrepareFunctionForOptimization(testDataViewDetachInvalidation);
assertEquals(38, testDataViewDetachInvalidation(dv_detach2, ab_dv2));
assertEquals(38, testDataViewDetachInvalidation(dv_detach2, ab_dv2));
%OptimizeFunctionOnNextCall(testDataViewDetachInvalidation);
assertEquals(38, testDataViewDetachInvalidation(dv_detach2, ab_dv2));
assertOptimized(testDataViewDetachInvalidation);
should_detach = true;
assertThrows(
    () => testDataViewDetachInvalidation(dv_detach2, ab_dv2), TypeError);
