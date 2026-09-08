// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  try {
    yield 1;
    yield 2;
    yield 3;
  } finally {
    yield "cleanup";
  }
}

function test(g) {
  return g.next();
}
%PrepareFunctionForOptimization(test);

let g = myGen();

// Warmup:
let res = test(g); // yields 1
assertEquals(1, res.value);
assertEquals(false, res.done);

// Optimization:
%OptimizeMaglevOnNextCall(test);
res = test(g); // yields 2
assertOptimized(test);
assertEquals(2, res.value);
assertEquals(false, res.done);

// Trigger the finally block
let resReturn = g.return("hello"); // yields "cleanup"
assertEquals("cleanup", resReturn.value);
assertEquals(false, resReturn.done);

// The finally block finishes, so it yields the stored return value ("hello")
res = test(g);
assertOptimized(test);
assertEquals("hello", res.value);
assertEquals(true, res.done);
