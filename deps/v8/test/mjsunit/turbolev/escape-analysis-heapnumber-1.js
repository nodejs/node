// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    assertEquals(bar.arguments[0].x, 6.05);
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  baz(deopt);
  o.x = "abc";
}

function foo(a, deopt) {
  let v = a + 4.5;
  let o = { x : v };
  bar(o, deopt);
  %AssertEscapeAnalysisElided(o);
  return o.x;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals("abc", foo(1.55, false));
assertEquals("abc", foo(1.55, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals("abc", foo(1.55, false));
assertOptimized(foo);

assertEquals("abc", foo(1.55, true));
assertUnoptimized(foo);
