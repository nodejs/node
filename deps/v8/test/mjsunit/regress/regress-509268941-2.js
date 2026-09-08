// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// As of this writing, this test does not flush out any issues. However, it
// is the JS version of a repro for a bug we had in Wasm Load Elimination,
// so the coverage it provides might prove useful if we ever implement similar
// optimizations for JS.

function foo(c, o, other) {

  let base = o.x;
  let initial_y = base.y;
  let ret = initial_y;

  let loop_phi = base;

  for (let i = 0; i < 42; i++) {
    // First iteration: no replacement for {loop_phi}.
    // Second iteration: {loop_phi} has {base} has replacement

    // First iteration: {inner_phi} will have {base} as replacement.
    // Second iteration: no replacement (because {o.x} gets invalidated later).
    let inner_phi = c ? o.x : base;

    // First iteration: not load eliminated because {loop_phi} has no
    // replacement.
    // Second iteration: replaced by {initial_y} since {loop_phi} is replaced
    // by {base}.
    ret = loop_phi.y;

    // Invalidate {o.x}, this will trigger initial revisit of the loop.
    o.x = other;

    loop_phi = inner_phi;
  }

  return ret;
}

let o1 = { x : { y : 17 } };
let o2 = { y : 29 };

%PrepareFunctionForOptimization(foo);
assertEquals(29, foo(true, o1, o2));

%OptimizeFunctionOnNextCall(foo);
assertEquals(29, foo(true, o1, o2));
