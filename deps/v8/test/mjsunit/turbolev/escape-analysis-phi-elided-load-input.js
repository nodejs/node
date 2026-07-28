// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax --no-maglev-loop-peeling --turbofan
// Flags: --turbolev-escape-analysis

function foo(b, n) {
  let phi = 42;
  if (b) {
    let o = { x : n };

    // Making {o.x} non-const.
    o.x += 1;

    // We make the GraphBuilder and GraphOptimizer lose track of the value of
    // {o.x} by using an empty loop with loop (s)peeling disabled, which
    // triggers some invalidations, leading to {o.x} being reloaded a few lines
    // below.
    for (let i = 0; i < 42; i++) { }

    // Reloading {o.x} and making it an input to the subsequent phi.
    phi = o.x;
    %AssertEscapeAnalysisElided(o);
  }
  return phi;
}

%PrepareFunctionForOptimization(foo);
assertEquals(18, foo(true, 17));
assertEquals(24, foo(true, 23));
assertEquals(42, foo(false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(18, foo(true, 17));
assertEquals(24, foo(true, 23));
assertEquals(42, foo(false));
