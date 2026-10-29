// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  yield 1;
  throw new Error("Boom");
}

function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

let generatorObject = myGen();

// Warmup:
let res = test(generatorObject); // yields 1
assertEquals(1, res.value);
assertEquals(false, res.done);

// Optimization:
%OptimizeMaglevOnNextCall(test);
assertThrows(() => test(generatorObject), Error, "Boom");

// Test that the generator is properly closed
res = test(generatorObject);
assertEquals(undefined, res.value);
assertEquals(true, res.done);
