// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test does not work well if we flush the feedback vector, which causes
// deoptimization.
// Flags: --no-stress-flush-code --no-flush-bytecode

function O() {}
O.prototype.f = f;
O.prototype.g = g;

function f() {
  return g.arguments;
}

function g(x) {
  return this.f(2 - x, "any");
}

var o = new O();
function foo(x) {
  return o.g(x, "z");
}

for (var i = 0; i < 35; i++) foo();

var result = (
  %PrepareFunctionForOptimization(foo),foo(), foo(),
  %OptimizeFunctionOnNextCall(foo), foo()
);
assertEquals(result[1], "z");
