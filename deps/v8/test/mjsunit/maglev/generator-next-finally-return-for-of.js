// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let finallyRan = false;

function* myGen() {
  finallyRan = false;
  try {
    yield 1;
    yield 2;
  } finally {
    finallyRan = true;
    yield "cleanup";
  }
}

function test(g) {
  let arr = [];
  for (let x of g) {
    arr.push(x);
    // Breaking out of a for-of loop early implicitly calls g.return() on the
    // iterator (via IteratorClose). This ensures the generator's finally block
    // executes, even though the for-of loop will discard any value yielded
    // from the finally block itself.
    if (x === 1) break;
  }
  return arr;
}
%PrepareFunctionForOptimization(test);

assertEquals([1], test(myGen()));
assertTrue(finallyRan);

assertEquals([1], test(myGen()));
assertTrue(finallyRan);

%OptimizeMaglevOnNextCall(test);
assertEquals([1], test(myGen()));
assertTrue(finallyRan);
assertOptimized(test);
