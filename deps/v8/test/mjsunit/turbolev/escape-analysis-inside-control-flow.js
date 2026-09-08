// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

// Test creating an object inside of a branch, which can be escaped analyzed
// away.

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x += 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.x += 5;
  baz(deopt);
}

function foo(arg, c, deopt) {
  let r = arg;
  if (c) {
    let o = { x : arg };
    bar(o, deopt);
    %AssertEscapeAnalysisElided(o);
    r += o.x;
  }
  return r * 2;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(84, foo(42, false, false));
assertEquals(178, foo(42, true, false));
assertEquals(178, foo(42, true, false));


%OptimizeFunctionOnNextCall(foo);
assertEquals(84, foo(42, false, false));
assertEquals(178, foo(42, true, false));
assertOptimized(foo);

assertEquals(212, foo(42, true, true));
assertUnoptimized(foo);
