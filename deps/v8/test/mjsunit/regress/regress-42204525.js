// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(arr, val, x) {
  // Use Int32Add to create an Int32 node in Maglev graph
  // that is larger than Smi::kMaxValue (1073741823) without deopting.
  let idx = 1000000000 + x;
  return arr.indexOf(val, idx);
}
%PrepareFunctionForOptimization(test);

let arr = [1, 2, 3];

// Warm up the function
test(arr, 1, 0);
test(arr, 1, 0);

%OptimizeMaglevOnNextCall(test);

// Pass a value that makes idx = 1073741824, which exceeds Smi::kMaxValue.
// In the buggy commit, this is passed as a HeapNumber to the ArrayIndexOfSmi
// CSA builtin, which expects a Smi, leading to a type cast failure (crash).
test(arr, 1, 73741824);
