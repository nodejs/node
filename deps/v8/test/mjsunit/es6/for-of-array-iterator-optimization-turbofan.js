// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --for-of-optimization

var results = [];

function testForOf(iterable) {
  results.length = 0;
  for (var i of iterable) {
    results.push(i);
  }
}

%PrepareFunctionForOptimization(testForOf);
for (let i = 0; i < 2; i++) {
  testForOf([100, 200, 300]);
  testForOf([100, 200, 300].keys());
  testForOf([100, 200, 300].entries());
  testForOf(new Uint8Array([100, 200, 300, 400, 500]));
}


%OptimizeFunctionOnNextCall(testForOf);
testForOf([1, 2, 3]);
assertEquals(results, [1, 2, 3]);


testForOf([1, 2, 3].keys());
assertEquals(results, [0, 1, 2]);


testForOf([1, 2, 3].entries());
assertEquals(results, [[0, 1], [1, 2], [2, 3]]);

testForOf(new Uint8Array([1, 2, 3, 4, 5]));
assertEquals(results, [1, 2, 3, 4, 5]);

assertOptimized(testForOf);

// Test missing 'done' property (ES6 treats missing done as falsy)
function testMissingDone(iterable) {
  let sum = 0;
  for (var i of iterable) {
    sum += i;
  }
  return sum;
}

const missingDoneIterable = {
  [Symbol.iterator]() {
    let count = 0;
    const iterator = {
      next() {
        if (count++ < 3) return { value: 10 };
        return { done: true };
      }
    };
    %NeverOptimizeFunction(iterator.next);
    return iterator;
  }
};

%PrepareFunctionForOptimization(testMissingDone);
for (let i = 0; i < 2; i++) {
  testMissingDone(missingDoneIterable);
}

%OptimizeFunctionOnNextCall(testMissingDone);
testMissingDone(missingDoneIterable);
assertEquals(30, testMissingDone(missingDoneIterable));

assertOptimized(testMissingDone);
