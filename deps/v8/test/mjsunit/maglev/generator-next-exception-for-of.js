// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  yield 1;
  throw new Error("myerror");
}

function test(g) {
  let sum = 0;
  for (let x of g) sum += x;
  return sum;
}
%PrepareFunctionForOptimization(test);

assertThrows(() => test(myGen()), Error, "myerror");
assertThrows(() => test(myGen()), Error, "myerror");

%OptimizeMaglevOnNextCall(test);
assertThrows(() => test(myGen()), Error, "myerror");
assertOptimized(test);
