// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* inner() {
  yield "inner 1";
  yield "inner 2";
}

function* outer() {
  yield "outer 1";
  let v = yield* inner();
  yield v;
}

function test(g) {
  try {
    return g.next();
  } catch (e) {}
}

%PrepareFunctionForOptimization(test);
let g = outer();
assertEquals("outer 1", test(g).value);

%OptimizeMaglevOnNextCall(test);
assertEquals("inner 1", test(g).value);
assertOptimized(test);

// Deoptimize while outer generator is suspended inside yield*:
%DeoptimizeFunction(test);
assertEquals("inner 2", test(g).value);

let res = test(g);
assertEquals(undefined, res.value);
assertEquals(false, res.done);
