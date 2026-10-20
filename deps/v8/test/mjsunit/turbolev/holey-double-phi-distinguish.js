// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --turbofan

// A loop phi carries holey-double values and has two uses: one that
// distinguishes the hole/undefined pattern from a genuine NaN (`x ===
// undefined`), and a Float64 arithmetic use (`x * 2`). The phi-representation
// selector untags such a phi to HoleyFloat64 (it can never become a silenced
// Float64, because any HoleyFloat64 input pins `possible_inputs` to
// {HoleyFloat64}), so the distinguishing use must still see holes as
// `undefined` and the real NaN as not-`undefined`.

function f(arr) {
  const n = arr.length;
  let x = arr[0];
  let holes = 0;
  let s = 0.0;
  for (let i = 1; i < n; i++) {
    if (x === undefined) holes++;  // distinguishes hole/undefined from NaN
    s += x * 2.0;                  // Float64 use (hole -> NaN)
    x = arr[i];                    // holey backedge
  }
  if (x === undefined) holes++;
  s += x * 2.0;
  return [holes, Number.isNaN(s)];
}

// HOLEY_DOUBLE array: a genuine NaN at index 1, holes at indices 2 and 4.
function mkArr() { return [1.5, NaN, /*hole*/, 2.5, /*hole*/, 3.5]; }

// The two holes count as `undefined`; the real NaN does not. `s` becomes NaN.
%PrepareFunctionForOptimization(f);
assertEquals([2, true], f(mkArr()));
assertEquals([2, true], f(mkArr()));
%OptimizeFunctionOnNextCall(f);
assertEquals([2, true], f(mkArr()));
assertOptimized(f);

// A hole-free HOLEY_DOUBLE array: no undefineds, `s` is a real number.
function mkDense() { let a = [1.5, 2.5, /*hole*/, 3.5]; a[2] = 4.5; return a; }
assertEquals([0, false], f(mkDense()));
assertOptimized(f);
