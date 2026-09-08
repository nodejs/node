// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x += 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, c, deopt) {
  if (c) {
    o.x = 12;
  }
  baz(deopt);
}

function foo(arg, c, deopt) {
  let o = { x : arg };
  bar(o, c, deopt);
  %AssertEscapeAnalysisElided(o);
  return o.x;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(42, false, false));
assertEquals(12, foo(42, true, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(42, false, false));
assertEquals(12, foo(42, true, false));
assertOptimized(foo);

assertEquals(29, foo(42, true, true));
assertUnoptimized(foo);
assertEquals(59, foo(42, false, true));
