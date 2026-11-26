// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev
// Flags: --no-optimize-on-next-call-optimizes-to-maglev
// Flags: --no-maglev-loop-peeling
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

// The index is first converted from a tagged value, which later turns out to
// be a Float64. A -0.0 index should neither deopt nor be observed as +0.0 by
// other uses.

const arr = [42, 43, 44, 45];

// The index is a loop phi that is untagged to Float64.
function loadWithPhiIndex(arr, x, n) {
  let idx = Math.floor(x);
  for (let i = 0; i < n; i++) idx = Math.floor(idx / 2);
  return {value: arr[idx], inverse: 1 / idx};
}

%PrepareFunctionForOptimization(loadWithPhiIndex);
assertEquals({value: 43, inverse: 1}, loadWithPhiIndex(arr, 3.5, 1));
assertEquals({value: 43, inverse: 1}, loadWithPhiIndex(arr, 7.5, 2));
assertEquals({value: 42, inverse: Infinity}, loadWithPhiIndex(arr, 1.5, 3));
%OptimizeFunctionOnNextCall(loadWithPhiIndex);
assertEquals({value: 42, inverse: Infinity}, loadWithPhiIndex(arr, 1.5, 1));
assertEquals({value: 42, inverse: -Infinity}, loadWithPhiIndex(arr, -0.0, 1));
assertOptimized(loadWithPhiIndex);

// The index is the Float64 result of a call that is inlined after graph
// building.
function floor(x) {
  return Math.floor(x);
}
function loadWithInlinedIndex(arr, x) {
  let idx = floor(x);
  return {value: arr[idx], inverse: 1 / idx};
}

%PrepareFunctionForOptimization(floor);
%PrepareFunctionForOptimization(loadWithInlinedIndex);
assertEquals({value: 43, inverse: 1}, loadWithInlinedIndex(arr, 1.5));
assertEquals({value: 44, inverse: 0.5}, loadWithInlinedIndex(arr, 2.5));
assertEquals({value: 42, inverse: Infinity}, loadWithInlinedIndex(arr, 0.5));
%OptimizeFunctionOnNextCall(loadWithInlinedIndex);
assertEquals({value: 42, inverse: Infinity}, loadWithInlinedIndex(arr, 0.5));
assertEquals({value: 42, inverse: -Infinity}, loadWithInlinedIndex(arr, -0.0));
assertOptimized(loadWithInlinedIndex);
