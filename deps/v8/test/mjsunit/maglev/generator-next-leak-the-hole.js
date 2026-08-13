// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test that we do not leak TheHole (which signals to skip IteratorResult
// allocation) when a generator suspends via yield*.

function* inner() {
  yield 2;
}
function* myGen() {
  yield 1;
  yield* inner();
  yield 3;
}

function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

let generatorObject = myGen();
// Warmup:
test(generatorObject); // yields 1

// Optimization:
%OptimizeMaglevOnNextCall(test);
test(generatorObject); // yields 2 (delegated to inner)

// At this point, yielded_value_ should be cleared.
// If it wasn't, the next call (unoptimized) will incorrectly see TheHole,
// skip allocating the IteratorResult, and return TheHole to JS space.
// This would then cause a crash when the JS code tries to access res.value.
let res = generatorObject.next();
assertEquals(3, res.value);
assertEquals(false, res.done);
