// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

function lazy_next() {
  this.count++;
  if (this.count === 4) {
    // Trigger a lazy deopt in the caller frame upon return!
    %DeoptimizeFunction(testForOfLazyDeoptNextCall);
  }
  if (this.count === 4) {
    return {done: true};
  }
  return {value: this.count * 10, done: false}; // Sums to 60
}
// Keep next() unoptimized to ensure a clean call/return boundary
// that correctly exercises the lazy deopt continuation.
%NeverOptimizeFunction(lazy_next);

const iterable = {
  [Symbol.iterator]() {
    return {
      count: 0,
      next: lazy_next
    };
  }
};

function testForOfLazyDeoptNextCall() {
  let sum = 0;
  for (const x of iterable) {
    sum += x;
  }
  return sum;
}

%PrepareFunctionForOptimization(testForOfLazyDeoptNextCall);
testForOfLazyDeoptNextCall();
testForOfLazyDeoptNextCall();

%OptimizeMaglevOnNextCall(testForOfLazyDeoptNextCall);
testForOfLazyDeoptNextCall();

const result = testForOfLazyDeoptNextCall();

assertUnoptimized(testForOfLazyDeoptNextCall);
assertEquals(60, result);
