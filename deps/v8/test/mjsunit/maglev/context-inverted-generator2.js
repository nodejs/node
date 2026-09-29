// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* f() {
  yield function g() {
    try { test } catch (e) { gen.next(); return test }
  }
  let test = 10;
}

var gen = f();
var g = gen.next().value;
%PrepareFunctionForOptimization(g);
assertEquals(10, g());

gen = f();
g = gen.next().value;
assertEquals(10, g());

gen = f();
g = gen.next().value;
assertEquals(10, g());

gen = f();
g = gen.next().value;
%OptimizeMaglevOnNextCall(g);
assertEquals(10, g());
