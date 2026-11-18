// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-eager-inlined-bytecode-size=0

function sloppyCallee() {}

// Inlined non-eagerly, so that the graph optimizer (rather than the graph
// builder) sees the constant call target and folds `z / d` to a deopt.
function getCallee(f) { return f; }
function getZero() { return 0; }

function inner(n, init, d, raw_f) {
  let f = getCallee(raw_f);
  let z = getZero();
  switch (init) {
    case 1:
      let x = "a";
    default:
      while (x, n-- > 0) {
        // The nested loop keeps the graph builder from peeling the outer one.
        do {} while (d > 9);
        x = "b";
        // 0 / -1 is -0: deopts, which removes the loop's backedge and turns
        // the loop phi for `x` into the TDZ hole constant.
        z / d;
      }
      // CheckedInternalizedString(tdz_hole).
      if (x === "a") {}
      return f.call(x);
  }
}

function outer(n) {
  return inner(n, 0, -1, sloppyCallee);
}

%PrepareFunctionForOptimization(getCallee);
%PrepareFunctionForOptimization(getZero);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(outer);
inner(2, 1, 1, sloppyCallee);
assertThrows(() => outer(2), ReferenceError);
%OptimizeFunctionOnNextCall(outer);
assertThrows(() => outer(2), ReferenceError);
