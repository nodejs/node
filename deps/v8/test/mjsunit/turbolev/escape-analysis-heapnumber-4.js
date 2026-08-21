// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x = 17.29;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.x = 12.37;
  baz(deopt);
}

function foo(arg, deopt) {
  let o = { x : arg };
  bar(o, deopt);
  %AssertEscapeAnalysisElided(o);
  return o.x + 3;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(15.37, foo(42.5, false));
assertEquals(15.37, foo(42.5, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(15.37, foo(42, false));
assertOptimized(foo);

assertEquals(20.29, foo(42, true));
assertUnoptimized(foo);
