// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan

// A peeled loop with loop-invariant typed-array and fixed-array accesses. The
// bounds checks and typed-array length load in the loop body are redundant
// with the ones in the peeled iteration; their elision must not lose the
// deopt for out-of-bounds indices.

function invariant_typed_load(f32, idx, n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += f32[idx];
    sum += i;
  }
  return sum;
}

const f32 = new Float32Array([1.5, 2.5, 3.5, 4.5]);
%PrepareFunctionForOptimization(invariant_typed_load);
assertEquals(70, invariant_typed_load(f32, 1, 10));
%OptimizeFunctionOnNextCall(invariant_typed_load);
assertEquals(70, invariant_typed_load(f32, 1, 10));
assertEquals(NaN, invariant_typed_load(f32, 100, 10));

function invariant_array_load(arr, idx, n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    sum += arr[idx];
    sum += i;
  }
  return sum;
}

const arr = [10, 20, 30, 40];
%PrepareFunctionForOptimization(invariant_array_load);
assertEquals(245, invariant_array_load(arr, 1, 10));
%OptimizeFunctionOnNextCall(invariant_array_load);
assertEquals(245, invariant_array_load(arr, 1, 10));
assertEquals(NaN, invariant_array_load(arr, 100, 10));

function invariant_typed_store(f32, idx, n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    f32[idx] = i;
    sum += f32[idx];
  }
  return sum;
}

%PrepareFunctionForOptimization(invariant_typed_store);
assertEquals(45, invariant_typed_store(new Float32Array(4), 1, 10));
%OptimizeFunctionOnNextCall(invariant_typed_store);
assertEquals(45, invariant_typed_store(new Float32Array(4), 1, 10));
assertEquals(NaN, invariant_typed_store(new Float32Array(4), 100, 10));
