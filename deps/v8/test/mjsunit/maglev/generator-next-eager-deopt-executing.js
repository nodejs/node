// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test eager deopt when the generator is currently executing
function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

let generatorObject;
function* myGen() {
  // This throws (because the generator already running) and catches the error.
  assertThrows(() => test(generatorObject), TypeError);
  yield 1;
}

// Warmup:
generatorObject = myGen();
// This first next() will start the generator, which calls test on itself
let res = generatorObject.next();
assertEquals(1, res.value);
assertEquals(false, res.done);

// Optimization:
generatorObject = myGen();
%OptimizeMaglevOnNextCall(test);

// Inside it calls test which will optimize and immediately deopt.
res = generatorObject.next();

assertEquals(1, res.value);
assertEquals(false, res.done);
assertUnoptimized(test);
