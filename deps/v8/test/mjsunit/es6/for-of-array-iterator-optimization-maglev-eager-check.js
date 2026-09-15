// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

const maps_normal = [
  {value: 10, done: false}, {value: 20, done: false},
  {value: undefined, done: true}
];

const maps_weird = [
  {value: 11, done: false}, {value: 21, done: false, extra: 'trigger deopt'},
  {value: undefined, done: true}
];

let triggerDeopt = false;

function getMap(iteration) {
  if (!triggerDeopt) {
    return maps_normal[iteration];
  }
  return maps_weird[iteration];
}
%NeverOptimizeFunction(getMap);

const iterable = {
  [Symbol.iterator]() {
    let iteration = 0;
    const iterator = {
      next() {
        return getMap(iteration++);
      }
    };
    %NeverOptimizeFunction(iterator.next);
    return iterator;
  }
};

function testForOfEagerDeopt() {
  let sum = 0;
  for (const x of iterable) {
    sum += x;
  }
  return sum;
}

// 1. Warm up. With triggerDeopt=false, getMap always returns from normal maps.
%PrepareFunctionForOptimization(testForOfEagerDeopt);
testForOfEagerDeopt();
testForOfEagerDeopt();

// 2. Optimize
%OptimizeMaglevOnNextCall(testForOfEagerDeopt);
testForOfEagerDeopt();

// 3. Trigger Eager Deopt on the SECOND iteration of a single call
// Iter 1: getMap(0) -> maps_weird[0] (value 11). sum = 11.
// Iter 2: getMap(1) -> maps_weird[1] (value 21). CHECKMAPS FAILS -> DEOPT.
// Interpreter resumes Iter 2: adds 21. sum = 32.
triggerDeopt = true;
const result = testForOfEagerDeopt();

assertUnoptimized(testForOfEagerDeopt);
assertEquals(32, result);
