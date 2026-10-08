// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(x, deopt) {
  let o = { x : x };
  %AssertEscapeAnalysisElided(o);

  // Creating a loop with no loop phis and with multiple predecessors,
  // and escape analysis will insert an EscapeAnalysisPhi inside.
  if (x == 2) {
    o.x = 3;
  } else {
    o.x = 4;
  }
  while (true) {
    if (o.x == 10) {
      break;
    }
    o.x = o.x + 1;
  }

  if (deopt) {
    x+3; // no feedback ==> unconditional deopt.
    return o;
  }

  return o.x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(10, foo(2, false));
assertEquals(10, foo(2, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(10, foo(2, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals({ x : 10 }, foo(2, true));
assertUnoptimized(foo);
