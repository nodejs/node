// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() { yield 1; }

function test(g) {
  let sum = 0;
  for (let x of g) sum += x;
  return sum;
}
%PrepareFunctionForOptimization(test);

let g = myGen();
let fake = {
  [Symbol.iterator]() {
    return {
      next: g.next
    };
  }
};

assertEquals(1, test(myGen()));
assertEquals(1, test(myGen()));

%OptimizeMaglevOnNextCall(test);
assertThrows(() => test(fake), TypeError);
assertUnoptimized(test);
