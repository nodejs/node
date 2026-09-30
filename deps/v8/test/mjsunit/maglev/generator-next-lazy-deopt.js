// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  yield 1;
  yield 2;
  %DeoptimizeFunction(test);
  yield 3;
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
res = test(generatorObject); // yields 2
assertEquals(2, res.value);
assertEquals(false, res.done);

res = test(generatorObject); // Deopts test and yields 3
assertUnoptimized(test);
assertEquals(3, res.value);
assertEquals(false, res.done);
