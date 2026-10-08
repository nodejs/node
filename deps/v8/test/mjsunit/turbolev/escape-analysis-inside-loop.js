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

function bar(o, deopt) {
  o.x += 2;
  baz(deopt);
}

function foo(arg, n, deopt) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    let o = { x : arg };
    bar(o, i == 3 && deopt);
    %AssertEscapeAnalysisElided(o);
    sum += o.x;
  }
  return sum;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(176, foo(42, 4, false));
assertEquals(440, foo(42, 10, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(176, foo(42, 4, false));
assertEquals(440, foo(42, 10, false));
assertOptimized(foo);

assertEquals(440+17, foo(42, 10, true));
assertUnoptimized(foo);
