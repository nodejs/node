// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

class C {
  constructor(x) { this.x = x; }
}

function foo(x, b, deopt) {
  x = x + 2;
  let o = new C(x);
  %AssertEscapeAnalysisElided(o);

  let v = o.x;

  // Transitioning in a single branch.
  if (b) {
    o.y = 15;
  }

  if (deopt) {
    if (!o.z) { return o; }
  }

  return o.x;
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertEquals(12, foo(10, true, false));
assertEquals(12, foo(10, false, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(12, foo(10, true, false));
assertEquals(12, foo(10, false, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals({x : 12}, foo(10, false, true));
assertUnoptimized(foo);
