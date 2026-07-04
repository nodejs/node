// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

class C {
  constructor(x) { this.x = x; }
}

function foo(x) {
  let o = new C(x);
  o.y = 15;
  %AssertEscapeAnalysisElided(o);
  return o.y;
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertEquals(15, foo(10));
assertEquals(15, foo(10));

%OptimizeFunctionOnNextCall(foo);
assertEquals(15, foo(10));
