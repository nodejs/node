// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

// Use a non-optimized function to process the iterated value to ensure we
// don't deopt for spurious reasons (e.g., unexpected value in operations
// used for processing).
function process(results, x) {
  results.push(x);
}
%NeverOptimizeFunction(process);

function testForOfTypedArray(iterable) {
  let results = [];
  for (var i of iterable) {
    process(results, i);
  }
  return results;
}

%PrepareFunctionForOptimization(testForOfTypedArray);

for (let i = 0; i < 2; i++) {
  assertEquals(
    [100, 200, 44, 144, 244],
    testForOfTypedArray(new Uint8Array([100, 200, 300, 400, 500]))
  );
}

%OptimizeMaglevOnNextCall(testForOfTypedArray);
assertEquals(
  [1, 2, 3, 4, 5],
  testForOfTypedArray(new Uint8Array([1, 2, 3, 4, 5]))
);

assertTrue(isMaglevved(testForOfTypedArray));
