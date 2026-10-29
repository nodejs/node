// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function test_array_at(arr) {
  return [
    arr.at(0),   // valid
    arr.at(1),   // valid
    arr.at(-1),  // valid
    arr.at(-2),  // valid
    arr.at(10),  // out-of-bounds
    arr.at(-10), // out-of-bounds
  ];
}

const arr = [10, 20, 30, 40];

%PrepareFunctionForOptimization(test_array_at);
let result = test_array_at(arr);
assertEquals([10, 20, 40, 30, undefined, undefined], result);

test_array_at(arr);
test_array_at(arr);

%OptimizeFunctionOnNextCall(test_array_at);
result = test_array_at(arr);
assertEquals([10, 20, 40, 30, undefined, undefined], result);
assertOptimized(test_array_at);
