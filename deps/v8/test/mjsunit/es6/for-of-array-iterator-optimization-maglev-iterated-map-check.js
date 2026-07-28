// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

function process(results, x, arrayToTransition) {
  results.push(x);
  if (x === 1 && arrayToTransition) {
    // Transition the array from PACKED_SMI_ELEMENTS to PACKED_DOUBLE_ELEMENTS
    arrayToTransition[1] = 1.5;
  }
}
%NeverOptimizeFunction(process);

function testForOf(iterable, arrayToTransition) {
  let results = [];
  for (var i of iterable) {
    process(results, i, arrayToTransition);
  }
  return results;
}

%PrepareFunctionForOptimization(testForOf);
testForOf([1, 2, 3], null);
testForOf([1, 2, 3], null);

%OptimizeMaglevOnNextCall(testForOf);
testForOf([1, 2, 3], null);

assertTrue(isMaglevved(testForOf));

// Run with an array that we will transition during iteration.
let transitionMe = [1, 2, 3];
let results = testForOf(transitionMe, transitionMe);

// Ensure we got the correct results despite the transition and deopt.
// The elements should be:
// 1st iteration: 1 (before transition)
// 2nd iteration: 1.5 (after transition, loaded as double)
// 3rd iteration: 3
assertEquals(results, [1, 1.5, 3]);

// It should have deoptimized because the map changed.
assertFalse(isMaglevved(testForOf));
