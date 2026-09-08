// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

function process(results, x) {
  results.push(x);
}
%NeverOptimizeFunction(process);

// Way 1: Iterator for Array used on TypedArray
function test1(iterable) {
  let results = [];
  for (let x of iterable) {
    process(results, x);
  }
  return results;
}

%PrepareFunctionForOptimization(test1);

let ta1 = new Uint8Array([10, 20, 30]);
let arr1 = [3, 4, 5];

// Monkey-patch TypedArray to return iterator for normal array.
ta1[Symbol.iterator] = function() { return arr1[Symbol.iterator](); };

assertEquals([3, 4, 5], test1(ta1));
assertEquals([3, 4, 5], test1(ta1));

%OptimizeMaglevOnNextCall(test1);
assertEquals([3, 4, 5], test1(ta1));
assertTrue(isMaglevved(test1));

// Way 2: Iterator for TypedArray used on Array.
function test2(iterable) {
  let results = [];
  for (let x of iterable) {
    process(results, x);
  }
  return results;
}

%PrepareFunctionForOptimization(test2);

let arr2 = [1, 2, 3];
let ta2 = new Uint8Array([44, 55, 66]);

// Monkey-patch normal array to return iterator for TypedArray.
arr2[Symbol.iterator] = function() { return ta2[Symbol.iterator](); };

assertEquals([44, 55, 66], test2(arr2));
assertEquals([44, 55, 66], test2(arr2));

%OptimizeMaglevOnNextCall(test2);
assertEquals([44, 55, 66], test2(arr2));
assertTrue(isMaglevved(test2));
