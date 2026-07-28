// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev --turbolev-escape-analysis

// This test is designed to catch a "missing revisit" bug if V8 ever supports
// merging escape analysis candidates in Phis without escaping them.
// Currently, candidates in Phis always escape, so this test will pass.
//
// If that optimization is added, and the loop revisit logic is not updated to
// check PatchLoopPhisBackedges changes, this test will fail (likely crash
// during compilation because a candidate is incorrectly elided while being
// passed to an escaping call).

function MyObj(val) {
  this.f = val;
}

globalThis.escaped = null;
function escape(obj) {
  globalThis.escaped = obj;
}

function foo() {
  let o1 = new MyObj(1);
  let o2 = new MyObj(2);
  let o_escape = new MyObj(3); // Should escape

  // o.f will hold o1 initially, o2 in iteration 0, and o_escape in iteration 1.
  let o = new MyObj(o1);

  for (let i = 0; i < 3; i++) {
    let current = o.f;

    // Escaping the Phi. If we support Phis of candidates, this should
    // escape all inputs of 'current' (o1, o2, o_escape).
    escape(current);

    if (i == 0) {
      o.f = o2;
    } else {
      o.f = o_escape;
    }
  }

  %DeoptimizeNow();
  return o_escape.f;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
assertEquals(3, foo());
assertEquals(3, globalThis.escaped.f);
