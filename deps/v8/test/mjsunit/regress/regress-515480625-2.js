// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turboshaft-loop-unrolling

// This is the JS version of a reproducer for a bug in WasmLoadElimination.
// At the time of landing this, we don't have a similar JS LoadElim, but if
// we ever implement one, we want to make sure it handles this case correctly.

function foo(o) {
  let base = o.x;

  // Creating 2 loop phis where
  //   - The 2nd one if the backedge of the 1st one (the order matters!)
  //   - The 2nd one's backedge is identical as its forward edge.

  // This means that {phi1} can be replaced by {base}, AFTER WHICH {phi1} can be
  // replaced by {base} as well. If we visit the loop header only once, then
  // only {phi2} gets replaced, whereas if we visit it twice in a row, then
  // during the 1st visit {phi2} gets replaced and during the 2nd one {phi1}
  // gets replaced as well.
  let phi1 = o.x;
  let phi2 = o.x;

  for (let i = 0; i < 42; i++) {
    phi1 = phi2;
    phi2 = o.x;
  }

  // {phi1} keeps {phi2} alive, so by returning {phi1} we keep both phis alive.
  return phi1;
}

let o = { x : "ab" };

%PrepareFunctionForOptimization(foo);
foo(o);

%OptimizeFunctionOnNextCall(foo);
foo(o);
