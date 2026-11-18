// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// 1. FixedArray and FixedDoubleArray element loads and stores:
// - LoadFixedArrayElement & StoreFixedArrayElementNoWriteBarrier (smiArr)
// - StoreFixedArrayElementWithWriteBarrier (objArr)
// - LoadFixedDoubleArrayElement & StoreFixedDoubleArrayElement (dblArr)
// - LoadHoleyFixedDoubleArrayElement & StoreFixedHoleyDoubleArrayElement (holeyDblArr)
function testFixedAndDoubleArrays(smiArr, objArr, dblArr, holeyDblArr, obj, idx) {
  let i = idx & 3;
  let next = (idx + 1) & 3;
  smiArr[i] = (smiArr[i] + 1) | 0;
  objArr[i] = obj;
  dblArr[i] = dblArr[i] + 0.5;
  holeyDblArr[i] = holeyDblArr[next];
  return smiArr[i] + dblArr[i] + (holeyDblArr[i] === undefined ? 10 : 0);
}

const smiArr = [1, 2, 3, 4];
const objArr = [{}, {}, {}, {}];
const dblArr = [1.5, 2.5, 3.5, 4.5];
const holeyDblArr = [1.5, , 3.5, ,];
const newObj = {x: 42};

%PrepareFunctionForOptimization(testFixedAndDoubleArrays);
assertEquals(14, testFixedAndDoubleArrays(smiArr, objArr, dblArr, holeyDblArr, newObj, 0));
assertEquals(6, testFixedAndDoubleArrays(smiArr, objArr, dblArr, holeyDblArr, newObj, 1));
%OptimizeFunctionOnNextCall(testFixedAndDoubleArrays);
assertEquals(5.5, testFixedAndDoubleArrays(smiArr, objArr, dblArr, holeyDblArr, newObj, 0));
assertSame(newObj, objArr[0]);
assertEquals(3.5, holeyDblArr[0]);

// 2. Off-heap constant TypedArray (byteLength = 128 > kMaxSizeInHeap = 64):
// Exercises IntPtrConstant length <= kMaxInt in ConvertTypedArrayIndexForBoundsCheck
// as well as BuildConstantTypedArrayLoad and BuildConstantTypedArrayStore.
const kOffHeapTypedArray = new Int32Array(32);
kOffHeapTypedArray[0] = 10;
kOffHeapTypedArray[7] = 20;

function testOffHeapConstantTypedArray(i) {
  kOffHeapTypedArray[i] = (kOffHeapTypedArray[i] + 3) | 0;
  return kOffHeapTypedArray[i];
}

%PrepareFunctionForOptimization(testOffHeapConstantTypedArray);
assertEquals(13, testOffHeapConstantTypedArray(0));
assertEquals(23, testOffHeapConstantTypedArray(7));
%OptimizeFunctionOnNextCall(testOffHeapConstantTypedArray);
assertEquals(16, testOffHeapConstantTypedArray(0));
// Negative index must still fail bounds check and return undefined after deopt.
assertEquals(undefined, testOffHeapConstantTypedArray(-1));
assertEquals(undefined, testOffHeapConstantTypedArray(-0x80000000));

// 3. Dynamic TypedArray with statically non-negative index (CheckedUint32ToInt32
// has GetStaticRange() == Range::Uint32() with min() == 0 >= 0) and
// BuildTypedArrayLoad / BuildTypedArrayStore.
function testDynamicTypedArrayNonNegativeIndex(ta, rawIdx) {
  let i = rawIdx >>> 0;
  ta[i] = (ta[i] + 5) | 0;
  return ta[i];
}

const dynTa = new Int32Array(16);
%PrepareFunctionForOptimization(testDynamicTypedArrayNonNegativeIndex);
assertEquals(5, testDynamicTypedArrayNonNegativeIndex(dynTa, 0));
assertEquals(5, testDynamicTypedArrayNonNegativeIndex(dynTa, 3));
%OptimizeFunctionOnNextCall(testDynamicTypedArrayNonNegativeIndex);
assertEquals(10, testDynamicTypedArrayNonNegativeIndex(dynTa, 0));
assertEquals(10, testDynamicTypedArrayNonNegativeIndex(dynTa, 3));

// 4. TypedArray OOB-handling mode (BranchIfTypedArrayBounds) with negative and
// in-bounds indices on both dynamic and constant-length TypedArrays.
function testTypedArrayOOBLoadStore(ta, i, val) {
  ta[i] = val;
  return ta[i];
}

const taOOB1 = new Int32Array(4);
const taOOB2 = new Int32Array(8);
%PrepareFunctionForOptimization(testTypedArrayOOBLoadStore);
assertEquals(11, testTypedArrayOOBLoadStore(taOOB1, 1, 11));
assertEquals(22, testTypedArrayOOBLoadStore(taOOB2, 2, 22));
assertEquals(undefined, testTypedArrayOOBLoadStore(taOOB1, 10, 99));
assertEquals(undefined, testTypedArrayOOBLoadStore(taOOB1, -1, 99));
%OptimizeFunctionOnNextCall(testTypedArrayOOBLoadStore);
assertEquals(33, testTypedArrayOOBLoadStore(taOOB1, 1, 33));
assertEquals(44, testTypedArrayOOBLoadStore(taOOB2, 5, 44));
assertEquals(undefined, testTypedArrayOOBLoadStore(taOOB1, -1, 99));
assertEquals(undefined, testTypedArrayOOBLoadStore(taOOB1, -0x80000000, 99));
assertEquals(undefined, testTypedArrayOOBLoadStore(taOOB2, 8, 99));
