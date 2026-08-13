// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(x, deopt) {
  let o1 = { o : 25, x : x };
  let o2 = { o : o1 };
  %AssertEscapeAnalysisElided(o1);
  %AssertEscapeAnalysisElided(o2);
  o1.o = o2;

  if (deopt) {
    return o1.o.o;
  }

  return o1.o.o.x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(42, false));
assertEquals(42, foo(42, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(42, false));
assertOptimized(foo);

// Triggering deopt.
let computed = foo(42, true);
// Not using assertEquals because it doesn't handle recursive objects.
assertSame(computed.o.o, computed);
assertEquals(42, computed.x);
assertUnoptimized(foo);
