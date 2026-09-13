// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

class C {
  constructor(x) { this.x = x; }
}

function foo(x, b, deopt) {
  let o1 = new C(x+2);
  let o2 = { o : o1, x : 42 };
  %AssertEscapeAnalysisElided(o1);
  %AssertEscapeAnalysisElided(o2);
  let v = o1.x;

  if (b) {
    // Updating {o.x} in a Phi to force an EscapeAnalysisPhi to be inserted.
    o1.x = 15;
  }

  if (deopt) {
    // The deopt state will contain {o2}, which means that it will also contain
    // {o1} (since it's a field in {o1}). Note however that the only reference
    // to {o1} is this field in {o2}, which means that if we don't recursively
    // update use-counts when updating framestates, {o1} will have a use-count
    // of 0 and so will its inputs, which will lead to the EscapeAnalysisPhi for
    // {o1.x} to be wrongly removed from the graph.
    return o2.o;
  }

  return v;
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertEquals(12, foo(10, false, false));
assertEquals(12, foo(10, true, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(12, foo(10, false, false));
assertEquals(12, foo(10, true, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals({ x : 15 }, foo(10, true, true));
assertUnoptimized(foo);
