// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test that a generator suspending via yield* does not skip result allocation.
// The body suspends inside the delegated iterator and never hits
// %GeneratorYieldResult.

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

let res = generatorObject.next();
assertEquals(3, res.value);
assertEquals(false, res.done);
