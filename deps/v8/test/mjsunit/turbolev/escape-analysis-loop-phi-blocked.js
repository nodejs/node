// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --no-turbolev-non-eager-loop-peeling --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x += 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.x += 3;
  baz(deopt);
}

function foo(arg, n, deopt1, deopt2) {
  let sum = 0;
  let o = { x : arg };
  %AssertEscapeAnalysisElided(o);
  for (let i = 0; i < n; i++) {
    bar(o, i == 0 && deopt1);
    sum += o.x;
    o = { x : i };
    bar(o, i == 3 && deopt2);
    o.x += 2;
    sum += o.x;
  }
  return sum;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(98, foo(42, 4, false, false));
assertEquals(248, foo(42, 10, false, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(98, foo(42, 4, false, false));
assertEquals(248, foo(42, 10, false, false));
assertOptimized(foo);

// Checking that the 1st deopt works (because the 1st allocation doesn't flow
// into the loop, but only into the peeled iteration).
assertEquals(248+17, foo(42, 10, true, false));
assertUnoptimized(foo);

// Note that we won't change anything when reopting, this is a known deopt loop.
%OptimizeFunctionOnNextCall(foo);
assertEquals(98, foo(42, 4, false, false));
assertEquals(248, foo(42, 10, false, false));
assertOptimized(foo);

// The 2nd deopt should not actually deopt, because the object allocated inside
// of the loop cannot be elided.
assertEquals(248+34, foo(42, 10, false, true));
assertOptimized(foo);
