// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

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
}


%OptimizeFunctionOnNextCall(testForOf);
testForOf([1, 2, 3]);
console.log(results);
assertEquals(results, [1, 2, 3]);


testForOf([1, 2, 3].keys());
console.log(results);
assertEquals(results, [0, 1, 2]);


testForOf([1, 2, 3].entries());
console.log(results);
assertEquals(results, [[0, 1], [1, 2], [2, 3]]);

assertOptimized(testForOf);
