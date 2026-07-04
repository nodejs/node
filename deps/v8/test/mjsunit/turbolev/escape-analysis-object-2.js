// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[2].x = 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.x = 12;
  baz(deopt);
}

function foo(arg, deopt) {
  let o = { x : arg };
  bar(o, deopt, o); // Calling with 1 extra argument.
  %AssertEscapeAnalysisElided(o);
  return o.x;
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
