// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function test_array_slice(arr) {
  return [
    arr.slice(),         // full copy
    arr.slice(0),        // from index 0
    arr.slice(0, undefined), // from index 0 to end
    arr.slice(1),        // from index 1
    arr.slice(1, 3),     // from index 1 to 3
    arr.slice(-2),       // last two elements
    arr.slice(10),       // out-of-bounds
    arr.slice(-10),      // out-of-bounds negative
  ];
}

const arr = [10, 20, 30, 40];
const expected = [arr, arr, arr, [20, 30, 40], [20, 30], [30, 40], [], arr];

%PrepareFunctionForOptimization(test_array_slice);

let result = test_array_slice(arr);
assertEquals(expected, result);

test_array_slice(arr);
test_array_slice(arr);

%OptimizeFunctionOnNextCall(test_array_slice);
result = test_array_slice(arr);

assertEquals(expected, result);
assertOptimized(test_array_slice);
