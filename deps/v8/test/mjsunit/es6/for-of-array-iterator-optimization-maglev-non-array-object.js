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

function testForOf(iterable) {
  let results = [];
  for (let i of iterable) {
    process(results, i);
  }
  return results;
}

%PrepareFunctionForOptimization(testForOf);
assertEquals([1, 2, 3], testForOf([1, 2, 3]));
assertEquals([1, 2, 3], testForOf([1, 2, 3]));

%OptimizeMaglevOnNextCall(testForOf);
assertEquals([1, 2, 3], testForOf([1, 2, 3]));
assertTrue(isMaglevved(testForOf));
// Now create a non-array object with fast elements

const nonArray = {};
nonArray[0] = 'a';
nonArray[1] = 'b';
nonArray.length = 2;

// Get an iterator for it. The iterator is a JSArrayIterator with the
// above object as iterated_object.
const iter1 = Array.prototype.values.call(nonArray);

assertEquals(['a', 'b'], testForOf(iter1));

// Deopted because we iterated something else than an array.
assertFalse(isMaglevved(testForOf));

// After deopt, feedback is polymorphic (array map + non-array map), so
// recompilation uses the generic fallback path and handles both without deopt.
%OptimizeMaglevOnNextCall(testForOf);
assertEquals([1, 2, 3], testForOf([1, 2, 3]));

const iter2 = Array.prototype.values.call(nonArray);
assertEquals(['a', 'b'], testForOf(iter2));

assertTrue(isMaglevved(testForOf));
