// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  return 42;
}

function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

// Warmup:
let generatorObject1 = myGen();
let res = test(generatorObject1);
assertEquals(42, res.value);
assertEquals(true, res.done);

// Optimization:
%OptimizeMaglevOnNextCall(test);

let generatorObject2 = myGen();
res = test(generatorObject2);
assertOptimized(test);
assertEquals(42, res.value);
assertEquals(true, res.done);

// Calling next again on the closed generator
res = test(generatorObject2);
assertOptimized(test);
assertEquals(undefined, res.value);
assertEquals(true, res.done);
