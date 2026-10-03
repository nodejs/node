// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].y.x = 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.y.x = 12;
  baz(deopt);
}

function foo(arg, deopt) {
  let o1 = { x : arg };
  let o2 = { y : o1 };
  bar(o2, deopt);
  %AssertEscapeAnalysisElided(o1);
  %AssertEscapeAnalysisElided(o2);
  return o2.y.x;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(12, foo(42, false));
assertEquals(12, foo(42, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(12, foo(42, false));
assertOptimized(foo);

assertEquals(17, foo(42, true));
assertUnoptimized(foo);
