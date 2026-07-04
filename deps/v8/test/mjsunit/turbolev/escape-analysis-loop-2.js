// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].y.x += 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.y.x += 12;
  baz(deopt);
}

function foo(arg, deopt) {
  let o1 = { x : arg };
  let o2 = { y : undefined };
  for (let i = 0; i < 5; i++) {
    o2.y = o1;
  }
  // Accessing {o1} through a load from {o2}.
  let o3 = { y : o2.y };
  bar(o3, deopt);
  %AssertEscapeAnalysisElided(o1);
  %AssertEscapeAnalysisElided(o2);
  %AssertEscapeAnalysisElided(o3);
  return o3.y.x + o2.y.x;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(108, foo(42, false));
assertEquals(108, foo(42, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(108, foo(42, false));
assertOptimized(foo);

assertEquals(142, foo(42, true));
assertUnoptimized(foo);
