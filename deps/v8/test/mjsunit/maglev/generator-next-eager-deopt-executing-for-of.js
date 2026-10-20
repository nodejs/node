// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let generatorObject;
function* myGen() {
  %PrepareFunctionForOptimization(test);
  assertThrows(() => test(generatorObject), TypeError);
  yield 1;
}

function test(g) {
  let sum = 0;
  for (let x of g) sum += x;
  return sum;
}

generatorObject = myGen();
%PrepareFunctionForOptimization(test);
assertEquals(1, test(generatorObject));

generatorObject = myGen();
%OptimizeMaglevOnNextCall(test);
assertEquals(1, test(generatorObject));
assertUnoptimized(test);
