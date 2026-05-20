// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function assertHasHole(array, index, message) {
  assertFalse(index in array, `${message}: element ${index} should be hole`);
}

function runTest(testFunc, expected, message, holeIndices = []) {
  // Warm up without triggering deopt path
  %PrepareFunctionForOptimization(testFunc);
  assertEquals(7, testFunc(5));
  assertEquals(7, testFunc(5));

  %OptimizeFunctionOnNextCall(testFunc);
  assertEquals(7, testFunc(5));
  assertOptimized(testFunc);

  // Trigger deopt path
  let result = testFunc(-1);
  assertUnoptimized(testFunc);
  assertArrayEquals(expected, result, message);
  for (const index of holeIndices) {
    assertHasHole(result, index, message);
  }
  assertTrue(%HasDoubleElements(result), `${message}: IsDoubleElements`);
}

// Test case 1: Array with NaN
function testNaN(n) {
  let arr = [NaN, 0/0, NaN];
  if (n < 0) {
    n + 5;
    return arr;
  }
  return n + 2;
}
runTest(testNaN, [NaN, NaN, NaN], "Test NaN");

// Test case 2: Array with Infinity
function testInfinity(n) {
  let arr = [Infinity, -Infinity, 1/0];
  if (n < 0) {
    n + 5;
    return arr;
  }
  return n + 2;
}
runTest(testInfinity, [Infinity, -Infinity, Infinity], "Test Infinity");

// Test case 3: Array with holes
function testHoles(n) {
  let arr = [1.1, , 3.3, , 5.5];
  if (n < 0) {
    n + 5;
    return arr;
  }
  return n + 2;
}
runTest(testHoles, [1.1, undefined, 3.3, undefined, 5.5], "Test Holes", [1, 3]);

// Test case 4: Mixed array
function testMixed(n) {
  let arr = [1.1, NaN, , Infinity, -0.0, 5.5];
  if (n < 0) {
    n + 5;
    return arr;
  }
  return n + 2;
}
runTest(testMixed, [1.1, NaN, undefined, Infinity, -0.0, 5.5], "Test Mixed", [2]);
