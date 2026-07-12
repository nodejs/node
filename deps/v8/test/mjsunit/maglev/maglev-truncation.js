// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-truncation

function TestFunc(fn, test_cases) {
  %PrepareFunctionForOptimization(fn);
  for (const [args, expected] of test_cases) {
    assertEquals(expected, fn(...args));
  }
  %OptimizeMaglevOnNextCall(fn);
  for (const [args, expected] of test_cases) {
    assertEquals(expected, fn(...args));
  }
}

// --- Test Bitwise OR ---
(function() {
  function BitwiseOR(a, b) {
    return ((a | 1) + (b | 1)) | 1;
  }
  const test_cases = [
    [[1.0, 2.0], 5],
    [[1.1, 2.9], 5],
    [[1.9, 2.1], 5],
    [[-1.1, -2.9], -1],
    [[NaN, 0], 3],
    [[Infinity, 0], 3],
    [[-Infinity, 0], 3],
    [[2**31 - 1, 0], -2147483647],
    [[-(2**31), 0], -2147483645],
  ];
  TestFunc(BitwiseOR, test_cases);
})();

// --- Test Bitwise OR Nested Add ---
(function() {
  function BitwiseOR(a, b) {
    return ((a | 1) + (b | 1)) + (a | 1) | 1;
  }
  const test_cases = [
    [[1.0, 2.0], 5],
    [[1.1, 2.9], 5],
    [[1.9, 2.1], 5],
    [[-1.1, -2.9], -3],
    [[NaN, 0], 3],
    [[Infinity, 0], 3],
    [[-Infinity, 0], 3],
    [[2**31 - 1, 0], -1],
    [[-(2**31), 0], 3],
  ];
  TestFunc(BitwiseOR, test_cases);
})();

// --- Test Bitwise OR Nested Add ---
// This should not truncate!
(function() {
  function BitwiseOR(a, b) {
    return ((a | 1) + (b | 1)) + a | 1;
  }
  const test_cases = [
    [[1.0, 2.0], 5],
    [[1.1, 2.9], 5],
    [[1.9, 2.1], 5],
    [[-1.1, -2.9], -3],
    [[NaN, 0], 1],
    [[Infinity, 0], 1],
    [[-Infinity, 0], 1],
    [[2**31 - 1, 0], -1],
    [[-(2**31), 0], 3],
  ];
  TestFunc(BitwiseOR, test_cases);
})();

// --- Test Bitwise OR Nested Div ---
(function() {
  function BitwiseOR(a, b) {
    return (a | 1) / (b | 1) | 1;
  }
  const test_cases = [
    [[1.0, 2.0], 1],
    [[1.1, 2.9], 1],
    [[1.9, 2.1], 1],
    [[-1.1, -2.9], 1],
    [[NaN, 0], 1],
    [[Infinity, 0], 1],
    [[-Infinity, 0], 1],
    [[2**31 - 1, 0], 2147483647],
    [[-(2**31), 0], -2147483647],
  ];
  TestFunc(BitwiseOR, test_cases);
})();

// --- Test Bitwise OR Nested Div ---
// This should not truncate!
(function() {
  function BitwiseOR(a, b) {
    return (((a | 1) / (b | 1)) / (a | 1)) | 1;
  }
  const test_cases = [
    [[1.0, 2.0], 1],
    [[1.1, 2.9], 1],
    [[1.9, 2.1], 1],
    [[-1.1, -2.9], -1],
    [[NaN, 0], 1],
    [[Infinity, 0], 1],
    [[-Infinity, 0], 1],
    [[2**31 - 1, 0], 1],
    [[-(2**31), 0], 1],
  ];
  TestFunc(BitwiseOR, test_cases);
})();

// --- Test Bitwise XOR ---
(function() {
  function BitwiseXOR(a, b) {
    return ((a | 1) + (b | 1)) ^ 1;
  }
  const test_cases = [
    [[1.0, 2.0], 5],
    [[1.1, 2.9], 5],
    [[1.9, 2.1], 5],
    [[-1.1, -2.9], -1],
    [[NaN, 0], 3],
    [[Infinity, 0], 3],
    [[-Infinity, 0], 3],
    [[2**31 - 1, 0], -2147483647],
    [[-(2**31), 0], -2147483645],
  ];
  TestFunc(BitwiseXOR, test_cases);
})();

// --- Test Bitwise AND ---
(function() {
  function BitwiseAND(a, b) {
    return ((a | 1) + (b | 1)) & 1;
  }
  const test_cases = [
    [[1.0, 2.0], 0],
    [[1.1, 2.9], 0],
    [[1.9, 2.1], 0],
    [[-1.1, -2.9], 0],
    [[NaN, 0], 0],
    [[Infinity, 0], 0],
    [[-Infinity, 0], 0],
    [[2**31 - 1, 0], 0],
    [[-(2**31), 0], 0],
  ];
  TestFunc(BitwiseAND, test_cases);
})();
