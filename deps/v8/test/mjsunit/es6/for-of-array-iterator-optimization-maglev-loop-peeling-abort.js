// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization --maglev-loop-peeling

function foo(array) {
  let sum = 0;
  for (let x of array) {
    sum += (typeof x === 'number' ? x : 0);
    // Transition elements kind inside the loop to trigger map-mismatch
    // in the peeled loop body compilation.
    array[0] = {};
  }
  return sum;
}

%PrepareFunctionForOptimization(foo);

// Warmup to trigger polymorphic feedback for ForOfNext
let arr1 = [1, 2, 3];
foo(arr1);

let arr2 = [1.5, 2.5, 3.5];
foo(arr2);

%OptimizeMaglevOnNextCall(foo);
let arr3 = [1, 2, 3];
assertEquals(6, foo(arr3));
