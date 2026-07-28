// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  return 42;
}

function test(g) {
  let sum = 0;
  for (let x of g) sum += x;
  return sum;
}
%PrepareFunctionForOptimization(test);

assertEquals(0, test(myGen()));
assertEquals(0, test(myGen()));

%OptimizeMaglevOnNextCall(test);
assertEquals(0, test(myGen()));
assertOptimized(test);
