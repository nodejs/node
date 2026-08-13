// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan --no-stress-maglev
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
  for (var i of iterable) {
    process(results, i);
  }
  return results;
}

%PrepareFunctionForOptimization(testForOf);
testForOf([1, 2, 3]);

%OptimizeMaglevOnNextCall(testForOf);
testForOf([1, 2, 3]);

assertTrue(isMaglevved(testForOf));

// Now monkey-patch %ArrayIteratorPrototype%.next
let proto = Object.getPrototypeOf([][Symbol.iterator]());
let original_next = proto.next;
let called = false;
proto.next = function() {
  called = true;
  return original_next.call(this);
};

testForOf([1, 2, 3]);

assertTrue(called);
assertFalse(isMaglevved(testForOf));

// Restore
proto.next = original_next;
