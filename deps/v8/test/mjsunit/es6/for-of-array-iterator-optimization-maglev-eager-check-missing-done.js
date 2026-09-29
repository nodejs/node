// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

const maps_normal = [
  {value: 10, done: false}, {value: 20, done: false},
  {value: undefined, done: true}
];

// The second object is completely missing the "done" property.
// It will cause an eager deopt, and the interpreter should treat the missing
// "done" as undefined -> falsy, so it should add 21.
const maps_missing_done = [
  {value: 11, done: false}, {value: 21}, // <--- No done!
  {value: undefined, done: true}
];

let triggerDeopt = false;

function getMap(iteration) {
  if (!triggerDeopt) {
    return maps_normal[iteration];
  }
  return maps_missing_done[iteration];
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

function testForOfEagerDeoptMissingDone() {
  let sum = 0;
  for (const x of iterable) {
    sum += x;
  }
  return sum;
}

// 1. Warm up. With triggerDeopt=false, getMap always returns from normal maps.
%PrepareFunctionForOptimization(testForOfEagerDeoptMissingDone);
testForOfEagerDeoptMissingDone();
testForOfEagerDeoptMissingDone();

// 2. Optimize
%OptimizeMaglevOnNextCall(testForOfEagerDeoptMissingDone);
testForOfEagerDeoptMissingDone();

// 3. Trigger Eager Deopt on the SECOND iteration
// Iter 1: getMap(0) -> maps_missing_done[0] (value 11). sum = 11.
// Iter 2: getMap(1) -> maps_missing_done[1] (value 21, missing done). CHECKMAPS FAILS -> DEOPT.
// Interpreter resumes Iter 2: treats missing done as falsy, adds 21. sum = 32.
triggerDeopt = true;
const result = testForOfEagerDeoptMissingDone();

assertUnoptimized(testForOfEagerDeoptMissingDone);
assertEquals(32, result);
