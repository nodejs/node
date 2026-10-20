// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  yield 1;
  yield 2;
  return { answer: 42 };
}

function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

let generatorObject = myGen();

// Warmup:
let res = test(generatorObject);
assertEquals(1, res.value);
assertEquals(false, res.done);

// Optimization:
%OptimizeMaglevOnNextCall(test);
res = test(generatorObject);
assertOptimized(test);
assertEquals(2, res.value);
assertEquals(false, res.done);

// The generator finishes; next() returns the value
// the generator returned.
res = test(generatorObject);
assertOptimized(test);
assertEquals(42, res.value.answer);
assertEquals(true, res.done);

// Calling next() on an already closed generator
res = test(generatorObject);
assertOptimized(test);
assertEquals(undefined, res.value);
assertEquals(true, res.done);
