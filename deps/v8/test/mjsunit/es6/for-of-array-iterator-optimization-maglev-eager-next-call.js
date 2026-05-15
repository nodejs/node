// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

function next_A() {
  this.count++;
  if (this.count === 4) return {done: true};
  return {value: this.count * 10, done: false};
}

function next_B() {
  this.count++;
  if (this.count === 4) return {done: true};
  return {value: this.count * 100, done: false};
}

function getIterator() {
  return this.iterObj;
}

// Forcefully keep these objects (and their maps) alive in the top-level scope
// to prevent GC from clearing the IC feedback and causing test flakiness.
const iterObjA = { count: 0, next: next_A };
const iterObjB = { next: next_B, count: 0 };

const iterA = { [Symbol.iterator]: getIterator, iterObj: iterObjA };
const iterB = { [Symbol.iterator]: getIterator, iterObj: iterObjB };

function testForOfFallbackToCSA(iterable) {
  let sum = 0;
  for (const x of iterable) {
    sum += x;
  }
  return sum;
}

%PrepareFunctionForOptimization(testForOfFallbackToCSA);
iterA.iterObj.count = 0;
testForOfFallbackToCSA(iterA);

%OptimizeMaglevOnNextCall(testForOfFallbackToCSA);
iterA.iterObj.count = 0;
testForOfFallbackToCSA(iterA);

assertTrue(isMaglevved(testForOfFallbackToCSA));


// Now trigger the deopt.
// When iterB is passed, the iterator returned by getIterator has a different map.
// This strictly triggers an eager deopt on the map check for the iterator
// BEFORE the .next() call, successfully falling back to the CSA ForOfNext.
iterB.iterObj.count = 0;
const result = testForOfFallbackToCSA(iterB);

assertEquals(600, result);
assertUnoptimized(testForOfFallbackToCSA);
