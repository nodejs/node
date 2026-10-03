// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

// Invalidate NoElementsProtector before optimizing.
Object.prototype[1] = 'element';

// Use a non-optimized function to process the iterated value to ensure we
// don't deopt for spurious reasons (e.g., unexpected value in operations
// used for processing).
function process(results, x) {
  results.push(x);
}
%NeverOptimizeFunction(process);

function testForOf(iterable) {
  let results = [];
  for (var i of iterable) {
    process(results, i);
  }
  return results;
}

%PrepareFunctionForOptimization(testForOf);
assertEquals([1, 'element', 3], testForOf([1, , 3]));
assertEquals([1, 'element', 3], testForOf([1, , 3]));

%OptimizeMaglevOnNextCall(testForOf);
assertEquals([1, 'element', 3], testForOf([1, , 3]));
assertTrue(isMaglevved(testForOf));
