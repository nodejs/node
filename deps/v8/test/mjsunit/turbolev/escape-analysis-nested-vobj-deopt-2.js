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
  %AssertEscapeAnalysisElided(o2);

  if (deopt) {
    // The deopt state will contain {o2}, which means that it will also contain
    // {o1} (since it's a field in {o1}).
    return o2.o;
  }

  return o1;
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertEquals(new C(12), foo(10, false, false));
assertEquals(new C(12), foo(10, true, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(new C(12), foo(10, false, false));
assertEquals(new C(12), foo(10, true, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals(new C(12), foo(10, true, true));
assertUnoptimized(foo);
