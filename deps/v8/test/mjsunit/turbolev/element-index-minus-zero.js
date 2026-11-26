// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev
// Flags: --no-optimize-on-next-call-optimizes-to-maglev

// A Float64 element index of -0.0 should not deopt under --turbolev, and must
// not pollute the int32 alternative for subsequent -0.0-sensitive uses.
function loadWithFloorIndex(arr, x) {
  let idx = Math.floor(x);
  let val = arr[idx];
  return [val, Object.is(idx, -0)];
}

const arr = [42, 43, 44, 45];
%PrepareFunctionForOptimization(loadWithFloorIndex);
assertEquals([42, false], loadWithFloorIndex(arr, 0.5));
assertEquals([43, false], loadWithFloorIndex(arr, 1.9));
%OptimizeFunctionOnNextCall(loadWithFloorIndex);
assertEquals([42, false], loadWithFloorIndex(arr, 0.2));
assertOptimized(loadWithFloorIndex);

// -0.0 index accesses element 0 without deopting, while preserving -0.0 in idx.
assertEquals([42, true], loadWithFloorIndex(arr, -0.0));
assertOptimized(loadWithFloorIndex);

// Also test a subsequent arithmetic use that depends on -0.0.
function loadThenDivide(arr, x) {
  let idx = Math.floor(x);
  let v = arr[idx];
  let inv = 1 / idx;
  return [v, inv];
}

%PrepareFunctionForOptimization(loadThenDivide);
assertEquals([43, 1], loadThenDivide(arr, 1.5));
assertEquals([44, 0.5], loadThenDivide(arr, 2.1));
assertEquals([42, Infinity], loadThenDivide(arr, 0.1));
%OptimizeFunctionOnNextCall(loadThenDivide);
assertEquals([42, Infinity], loadThenDivide(arr, 0.1));
assertEquals([42, -Infinity], loadThenDivide(arr, -0.0));
assertOptimized(loadThenDivide);

// A subsequent Int32 use of the index must still deopt on -0.0, rather than
// reuse the index conversion.
function loadThenMultiply(arr, x) {
  let idx = Math.floor(x);
  let v = arr[idx];
  return [v, 1 / (idx * 2)];
}

%PrepareFunctionForOptimization(loadThenMultiply);
assertEquals([43, 0.5], loadThenMultiply(arr, 1.5));
assertEquals([44, 0.25], loadThenMultiply(arr, 2.5));
assertEquals([42, Infinity], loadThenMultiply(arr, 0.5));
%OptimizeFunctionOnNextCall(loadThenMultiply);
assertEquals([42, Infinity], loadThenMultiply(arr, 0.5));
assertOptimized(loadThenMultiply);
assertEquals([42, -Infinity], loadThenMultiply(arr, -0.0));

// HoleyFloat64 element index: -0.0 works, hole (NaN) still deopts/returns
// undefined.
function loadWithHoleyFloat64Index(arr, holey, i) {
  let idx = holey[i];
  return arr[idx];
}

const holey = [0.0, , -0.0, 2.0];
%PrepareFunctionForOptimization(loadWithHoleyFloat64Index);
assertEquals(42, loadWithHoleyFloat64Index(arr, holey, 0));
assertEquals(44, loadWithHoleyFloat64Index(arr, holey, 3));
%OptimizeFunctionOnNextCall(loadWithHoleyFloat64Index);
assertEquals(42, loadWithHoleyFloat64Index(arr, holey, 2));
assertOptimized(loadWithHoleyFloat64Index);
assertEquals(undefined, loadWithHoleyFloat64Index(arr, holey, 1));
