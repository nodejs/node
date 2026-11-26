// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

const cases = [
  { input: 0, expected: 0 },
  { input: -0, expected: -0 },
  { input: 1.5, expected: 1.5 },
  { input: 1 + 2 ** -11 + 2 ** -52, expected: 1 + 2 ** -10 },
  { input: 2049, expected: 2048 },
  { input: 2051, expected: 2052 },
  { input: 0.00006103515625, expected: 0.00006103515625 },
  { input: 5.960464477539063e-8, expected: 5.960464477539063e-8 },
  { input: 65504, expected: 65504 },
  { input: 65520, expected: Infinity },
  { input: -65520, expected: -Infinity },
  { input: NaN, expected: NaN },
  { input: undefined, expected: NaN },
];

function rawGetTypedArray(arr, i) {
  return arr[i];
}
function rawSetTypedArray(arr, i, v) {
  arr[i] = v;
}
function rawGetDataView(dv, offset, le) {
  return dv.getFloat16(offset, le);
}
function rawSetDataView(dv, offset, v, le) {
  dv.setFloat16(offset, v, le);
}
%NeverOptimizeFunction(rawGetTypedArray);
%NeverOptimizeFunction(rawSetTypedArray);
%NeverOptimizeFunction(rawGetDataView);
%NeverOptimizeFunction(rawSetDataView);

// 1. Non-constant Float16Array element load and store.
function storeTypedArray(arr, i, v) {
  arr[i] = v;
}
function loadTypedArray(arr, i) {
  return arr[i];
}

const arr1 = new Float16Array(8);
const arr2 = new Float16Array(8);
%PrepareFunctionForOptimization(storeTypedArray);
%PrepareFunctionForOptimization(loadTypedArray);
storeTypedArray(arr1, 0, 1.5);
storeTypedArray(arr2, 0, undefined);
assertEquals(1.5, loadTypedArray(arr1, 0));
assertEquals(NaN, loadTypedArray(arr2, 0));

%OptimizeFunctionOnNextCall(storeTypedArray);
%OptimizeFunctionOnNextCall(loadTypedArray);
for (const { input, expected } of cases) {
  storeTypedArray(arr1, 1, input);
  assertEquals(expected, rawGetTypedArray(arr1, 1));
  rawSetTypedArray(arr1, 2, input);
  assertEquals(expected, loadTypedArray(arr1, 2));
}
assertOptimized(storeTypedArray);
assertOptimized(loadTypedArray);

// Out-of-bounds accesses must deopt through the inlined bounds check.
assertEquals(undefined, loadTypedArray(arr1, 8));
assertUnoptimized(loadTypedArray);
storeTypedArray(arr1, 8, 1.5);
assertUnoptimized(storeTypedArray);

// 2. Constant off-heap Float16Array element load and store.
const constArr = new Float16Array(new ArrayBuffer(128));
function storeConstTypedArray(i, v) {
  constArr[i] = v;
}
function loadConstTypedArray(i) {
  return constArr[i];
}

%PrepareFunctionForOptimization(storeConstTypedArray);
%PrepareFunctionForOptimization(loadConstTypedArray);
storeConstTypedArray(0, 1.5);
storeConstTypedArray(1, undefined);
assertEquals(1.5, loadConstTypedArray(0));

%OptimizeFunctionOnNextCall(storeConstTypedArray);
%OptimizeFunctionOnNextCall(loadConstTypedArray);
for (const { input, expected } of cases) {
  storeConstTypedArray(2, input);
  assertEquals(expected, rawGetTypedArray(constArr, 2));
  rawSetTypedArray(constArr, 3, input);
  assertEquals(expected, loadConstTypedArray(3));
}
assertOptimized(storeConstTypedArray);
assertOptimized(loadConstTypedArray);

assertEquals(undefined, loadConstTypedArray(64));
assertUnoptimized(loadConstTypedArray);
storeConstTypedArray(64, 1.5);
assertUnoptimized(storeConstTypedArray);

// 3. DataView.prototype.getFloat16 and DataView.prototype.setFloat16.
const view = new DataView(new ArrayBuffer(16));
function storeDataView(dv, offset, v, le) {
  dv.setFloat16(offset, v, le);
}
function loadDataView(dv, offset, le) {
  return dv.getFloat16(offset, le);
}
function storeDataViewLE(dv, offset, v) {
  dv.setFloat16(offset, v, true);
}
function loadDataViewLE(dv, offset) {
  return dv.getFloat16(offset, true);
}
function storeDataViewBE(dv, offset, v) {
  dv.setFloat16(offset, v, false);
}
function loadDataViewBE(dv, offset) {
  return dv.getFloat16(offset, false);
}
function storeDataViewMissingValue(dv, offset) {
  dv.setFloat16(offset);
}

%PrepareFunctionForOptimization(storeDataView);
%PrepareFunctionForOptimization(loadDataView);
%PrepareFunctionForOptimization(storeDataViewLE);
%PrepareFunctionForOptimization(loadDataViewLE);
%PrepareFunctionForOptimization(storeDataViewBE);
%PrepareFunctionForOptimization(loadDataViewBE);
%PrepareFunctionForOptimization(storeDataViewMissingValue);
storeDataView(view, 0, 1.5, true);
storeDataView(view, 2, undefined, false);
storeDataViewLE(view, 0, 1.5);
storeDataViewBE(view, 2, undefined);
storeDataViewMissingValue(view, 4);
assertEquals(1.5, loadDataView(view, 0, true));
assertEquals(1.5, loadDataViewLE(view, 0));
assertEquals(NaN, loadDataViewBE(view, 2));

%OptimizeFunctionOnNextCall(storeDataView);
%OptimizeFunctionOnNextCall(loadDataView);
%OptimizeFunctionOnNextCall(storeDataViewLE);
%OptimizeFunctionOnNextCall(loadDataViewLE);
%OptimizeFunctionOnNextCall(storeDataViewBE);
%OptimizeFunctionOnNextCall(loadDataViewBE);
%OptimizeFunctionOnNextCall(storeDataViewMissingValue);
for (const { input, expected } of cases) {
  for (const le of [true, false]) {
    storeDataView(view, 1, input, le);
    assertEquals(expected, rawGetDataView(view, 1, le));
    rawSetDataView(view, 3, input, le);
    assertEquals(expected, loadDataView(view, 3, le));
  }
  storeDataViewLE(view, 1, input);
  assertEquals(expected, rawGetDataView(view, 1, true));
  rawSetDataView(view, 3, input, true);
  assertEquals(expected, loadDataViewLE(view, 3));

  storeDataViewBE(view, 1, input);
  assertEquals(expected, rawGetDataView(view, 1, false));
  rawSetDataView(view, 3, input, false);
  assertEquals(expected, loadDataViewBE(view, 3));
}
rawSetDataView(view, 4, 1.5, false);
storeDataViewMissingValue(view, 4);
assertEquals(NaN, rawGetDataView(view, 4, false));
assertOptimized(storeDataView);
assertOptimized(loadDataView);
assertOptimized(storeDataViewLE);
assertOptimized(loadDataViewLE);
assertOptimized(storeDataViewBE);
assertOptimized(loadDataViewBE);
assertOptimized(storeDataViewMissingValue);

assertThrows(() => loadDataView(view, 15, true), RangeError);
assertUnoptimized(loadDataView);
assertThrows(() => storeDataView(view, 15, 1.5, true), RangeError);
assertUnoptimized(storeDataView);
