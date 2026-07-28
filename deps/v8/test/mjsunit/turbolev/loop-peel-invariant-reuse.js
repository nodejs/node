// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

// A loop-invariant value converted inside a non-eager-peeled loop is cloned for
// the peeled iteration, leaving the peeled copy (which dominates the loop) and
// the in-loop copy as two structurally-identical nodes. The KNA recompute's
// MergeForLoop keeps the dominating front-edge copy across the loop header so
// the optimizer reuses it. These tests check the reuse stays value-preserving.

// Invariant `n` converted to Float64 in the body (`s + n`).
function add_invariant_float(n) {
  let s = 0.5;
  for (let i = 0; i < 100; i++) {
    s = s + n;
  }
  return s;
}
%PrepareFunctionForOptimization(add_invariant_float);
assertEquals(250.5, add_invariant_float(2.5));
assertEquals(250.5, add_invariant_float(2.5));
%OptimizeFunctionOnNextCall(add_invariant_float);
assertEquals(150.5, add_invariant_float(1.5));
assertEquals(450.5, add_invariant_float(4.5));
assertEquals(250.5, add_invariant_float(2.5));
assertOptimized(add_invariant_float);

// Invariant `n` used in two Float64 subexpressions per iteration.
function two_uses(n) {
  let s = 0.0;
  for (let i = 0; i < 50; i++) {
    s = s + n * 2.0 + n;
  }
  return s;
}
%PrepareFunctionForOptimization(two_uses);
assertEquals(375, two_uses(2.5));
assertEquals(375, two_uses(2.5));
%OptimizeFunctionOnNextCall(two_uses);
assertEquals(225, two_uses(1.5));
assertEquals(525, two_uses(3.5));
assertEquals(375, two_uses(2.5));
assertOptimized(two_uses);

// Invariant `n` used in Int32 arithmetic in the body.
function int_invariant(n) {
  let s = 0;
  for (let i = 0; i < 64; i++) {
    s = (s + i * n) | 0;
  }
  return s;
}
%PrepareFunctionForOptimization(int_invariant);
assertEquals(6048, int_invariant(3));
assertEquals(6048, int_invariant(3));
%OptimizeFunctionOnNextCall(int_invariant);
assertEquals(4032, int_invariant(2));
assertEquals(14112, int_invariant(7));
assertEquals(6048, int_invariant(3));
assertOptimized(int_invariant);
