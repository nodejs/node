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

function bar(o1, o2, n, deopt1, deopt2) {
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      o1.x = o2.y.x;
      if (i == 3 && j == 3) {
        baz(deopt1);
      }
    }
    o1.x += i;
  }
  baz(deopt2);
}

function foo(arg, n, deopt1, deopt2) {
  let o1 = { x : arg };
  let o2 = { y : o1 };
  bar(o1, o2, n, deopt1, deopt2);
  %AssertEscapeAnalysisElided(o1);
  %AssertEscapeAnalysisElided(o2);
  return o1.x;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(48, foo(42, 4, false, false));
assertEquals(87, foo(42, 10, false, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(48, foo(42, 4, false, false));
assertEquals(87, foo(42, 10, false, false));
assertOptimized(foo);

// Checking that the 1st deopt works (inside the loop).
assertEquals(104, foo(42, 10, true, false));
assertUnoptimized(foo);

// Note that we won't change anything when reopting, this is a known deopt loop.
%OptimizeFunctionOnNextCall(foo);
assertEquals(48, foo(42, 4, false, false));
assertEquals(87, foo(42, 10, false, false));
assertOptimized(foo);

// Checking that the 2nd deopt works (after the loop).
assertEquals(104, foo(42, 10, false, true));
assertUnoptimized(foo);
