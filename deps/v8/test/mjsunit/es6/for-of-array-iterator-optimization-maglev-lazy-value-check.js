// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

let deopt_now = false;

const proto = {
  done: false
};

Object.defineProperty(proto, 'value', {
  get: function() {
    if (deopt_now) {
     // Trigger lazy deopt right here during the 'value' check
     %DeoptimizeFunction(testForOfDeopt);
     deopt_now = false;
     }
    return 42;
  },
  configurable: true
});

const iterable = {
  [Symbol.iterator]() {
    let count = 0;
    deopt_now = false;
    const iterator = {
      next() {
        count++;
        if (count === 3) {
          // Trigger deopt on the 3rd iteration.
          deopt_now = true;
        }
        if (count > 3) {
          return {done: true};
        }
        return Object.create(proto);
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

%PrepareFunctionForOptimization(testForOfDeopt);
testForOfDeopt();
testForOfDeopt();

%OptimizeMaglevOnNextCall(testForOfDeopt);

const result = testForOfDeopt();
assertUnoptimized(testForOfDeopt);
assertEquals(126, result);
