// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

function testForOf(iterable) {
  let results = [];
  let resultsIx = 0;
  for (var i of iterable) {
    // Don't use Array.p.push since it does its own speculative optimizations.
    results[resultsIx++] = i;
  }
  return results;
}

%PrepareFunctionForOptimization(testForOf);
assertEquals([1, undefined, 3], testForOf([1, , 3]));
assertEquals([1, undefined, 3], testForOf([1, , 3]));

%OptimizeMaglevOnNextCall(testForOf);
assertEquals([1, undefined, 3], testForOf([1, , 3]));
assertTrue(isMaglevved(testForOf));

// Invalidate NoElementsProtector after optimizing.
Object.prototype[1] = 'element';

assertEquals([1, 'element', 3], testForOf([1, , 3]));
assertFalse(isMaglevved(testForOf));
