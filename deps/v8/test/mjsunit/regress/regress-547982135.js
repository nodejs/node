// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert-types

function testIndexOf(arr, val, x) {
  let idx = 1000000000 + x;
  return arr.indexOf(val, idx);
}
%PrepareFunctionForOptimization(testIndexOf);

function testIncludes(arr, val, x) {
  let idx = 1000000000 + x;
  return arr.includes(val, idx);
}
%PrepareFunctionForOptimization(testIncludes);

let arr = [1, 2, 3];

testIndexOf(arr, 1, 0);
testIndexOf(arr, 1, 0);
testIncludes(arr, 1, 0);
testIncludes(arr, 1, 0);

%OptimizeMaglevOnNextCall(testIndexOf);
%OptimizeMaglevOnNextCall(testIncludes);

assertEquals(-1, testIndexOf(arr, 1, 73741824));
assertEquals(false, testIncludes(arr, 1, 73741824));
