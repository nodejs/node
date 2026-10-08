// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* myGen() {
  let val1 = yield 1;
  assertEquals("hello", val1);
  let val2 = yield 2;
  assertEquals("world", val2);
  let val3 = yield 3;
  assertEquals(undefined, val3);
}

function test(g, val) {
  return g.next(val);
}
%PrepareFunctionForOptimization(test);

let generatorObject = myGen();

// Warmup:
let res = test(generatorObject); // yields 1, argument ignored since generator just started
assertEquals(1, res.value);
assertEquals(false, res.done);

res = test(generatorObject, "hello"); // passes "hello" to val1, yields 2
assertEquals(2, res.value);
assertEquals(false, res.done);

// Optimization:
%OptimizeMaglevOnNextCall(test);
res = test(generatorObject, "world"); // optimized, passes "world" to val2, yields 3
assertOptimized(test);
assertEquals(3, res.value);
assertEquals(false, res.done);

// Pass undefined explicitly
res = test(generatorObject, undefined); // optimized, passes undefined to val3, finishes
assertOptimized(test);
assertEquals(undefined, res.value);
assertEquals(true, res.done);
