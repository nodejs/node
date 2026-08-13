// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

let deopt_now = false;

// Use a single object to guarantee a stable Map (avoids eager map deopts)
const result_obj = {
  value: 100
};
Object.defineProperty(result_obj, 'done', {
  get: function() {
    if (deopt_now) {
      // Trigger lazy deopt right here during the 'done' check
      %DeoptimizeFunction(testForOfDeopt);
      deopt_now = false;
      return false;
    }
    return false;
  },
  configurable: true
});

const iterable = {
  [Symbol.iterator]() {
    let count = 0;
    const iterator = {
      next() {
        if (count === 3) return {done: true};
        count++;
        return result_obj;
      }
    };
    %NeverOptimizeFunction(iterator.next);
    return iterator;
  }
};

function testForOfDeopt() {
  let sum = 0;
  for (const x of iterable) {
    sum += x;
  }
  return sum;
}

// 1. Warm up cleanly (3 iterations of 100 = 300)
%PrepareFunctionForOptimization(testForOfDeopt);
testForOfDeopt();
testForOfDeopt();

// 2. Optimize
%OptimizeMaglevOnNextCall(testForOfDeopt);
testForOfDeopt();

// 3. Trigger Lazy Deopt on the very first iteration of this call
deopt_now = true;
const result = testForOfDeopt();

assertUnoptimized(testForOfDeopt);
assertEquals(300, result);
