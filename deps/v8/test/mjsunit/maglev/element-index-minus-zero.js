// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

const arr = [42, 43, 44, 45];

// A Float64 element index of -0.0 loads element 0 without deopting, and other
// uses of the index still observe -0.0.
function loadWithFloorIndex(arr, x) {
  let idx = Math.floor(x);
  return {value: arr[idx], inverse: 1 / idx};
}

%PrepareFunctionForOptimization(loadWithFloorIndex);
assertEquals({value: 43, inverse: 1}, loadWithFloorIndex(arr, 1.5));
assertEquals({value: 42, inverse: Infinity}, loadWithFloorIndex(arr, 0.5));
%OptimizeMaglevOnNextCall(loadWithFloorIndex);
assertEquals({value: 42, inverse: Infinity}, loadWithFloorIndex(arr, 0.5));
assertEquals({value: 42, inverse: -Infinity}, loadWithFloorIndex(arr, -0.0));
assertTrue(isMaglevved(loadWithFloorIndex));
assertEquals({value: undefined, inverse: 0}, loadWithFloorIndex(arr, Infinity));
assertFalse(isMaglevved(loadWithFloorIndex));

// Same for a HoleyFloat64 index. The hole still deopts.
function loadWithHoleyIndex(arr, holey, i) {
  let idx = holey[i];
  return arr[idx];
}

const holey = [0.0, , -0.0, 2.0];
%PrepareFunctionForOptimization(loadWithHoleyIndex);
assertEquals(42, loadWithHoleyIndex(arr, holey, 0));
assertEquals(44, loadWithHoleyIndex(arr, holey, 3));
%OptimizeMaglevOnNextCall(loadWithHoleyIndex);
assertEquals(42, loadWithHoleyIndex(arr, holey, 2));
assertTrue(isMaglevved(loadWithHoleyIndex));
assertEquals(undefined, loadWithHoleyIndex(arr, holey, 1));
assertFalse(isMaglevved(loadWithHoleyIndex));
