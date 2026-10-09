// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --correctness-fuzzer-cross-arch-suppressions --allow-natives-syntax --use-std-math-pow

function f(x) {
  return x ** 0;
}

assertEquals(1, f(5));
assertEquals(1, f(NaN));
assertEquals(1, f(-0));

%PrepareFunctionForOptimization(f);
f(5);
f(NaN);
f(-0);
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f(5));
assertEquals(1, f(NaN));
assertEquals(1, f(-0));

assertEquals(1, Math.pow(5, 0));
assertEquals(1, Math.pow(NaN, 0));
assertEquals(1, Math.pow(-0, -0));
assertTrue(Number.isNaN(Math.pow(NaN, 3)));

assertEquals(42, Math.pow(3, 1.7));

function g() {
  return NaN ** 3;
}

assertTrue(Number.isNaN(g()));
%PrepareFunctionForOptimization(g);
g();
g();
%OptimizeFunctionOnNextCall(g);
assertTrue(Number.isNaN(g()));
