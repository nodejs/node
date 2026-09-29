// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* inner() {
  yield "inner value 1";
  yield "inner value 2";
  return "inner done";
}

function* myGen() {
  yield "value 1";
  let res = yield* inner();
  yield res;
  return "outer done";
}

function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

let generatorObject = myGen();

// Warmup:
let res = test(generatorObject);
assertEquals("value 1", res.value);
assertEquals(false, res.done);

// Optimization:
%OptimizeMaglevOnNextCall(test);
res = test(generatorObject);
assertOptimized(test);
assertEquals("inner value 1", res.value);
assertEquals(false, res.done);

res = test(generatorObject);
assertOptimized(test);
assertEquals("inner value 2", res.value);
assertEquals(false, res.done);

// Yields the result of yield* evaluation
res = test(generatorObject);
assertOptimized(test);
assertEquals("inner done", res.value);
assertEquals(false, res.done);

// Finishes the outer generator
res = test(generatorObject);
assertOptimized(test);
assertEquals("outer done", res.value);
assertEquals(true, res.done);
