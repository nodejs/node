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

function foo(iterable) {
  let results = [];
  for (let x of iterable) {
    process(results, x);
  }
  return results;
}

function custom_next() {
  if (this.count === undefined) this.count = 0;
  if (this.count >= 1) return { done: true };
  this.count++;
  return { value: 100, done: false };
}

function create_iterator() {
  let it = [1, 2, 3][Symbol.iterator]();
  it.next = custom_next;
  return it;
}

%PrepareFunctionForOptimization(foo);
assertEquals([100], foo(create_iterator()));
assertFalse(isMaglevved(foo));

assertEquals([100], foo(create_iterator()));

%OptimizeMaglevOnNextCall(foo);
assertEquals([100], foo(create_iterator()));

assertTrue(isMaglevved(foo));
