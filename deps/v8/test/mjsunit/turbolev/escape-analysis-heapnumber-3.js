// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x += 5;
    bar.arguments[0].y += 3;
    assertEquals(13.35+13.35+5+3, bar.arguments[0].x + bar.arguments[0].y);
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  // We create 2 fields with the same value, but {baz} will eventually set both
  // values to be different. Bugs in value numbering could merge the 2
  // HeapNumbers, which would lead to a wrong result after {baz}.
  baz(deopt);
  o.x = 12.15;
  o.y = 14.50;
}

function foo(deopt) {
  let o = { x : 13.35, y : 13.35 };
  bar(o, deopt);
  %AssertEscapeAnalysisElided(o);
  return o.x + o.y;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(12.15+14.50, foo(false));
assertEquals(12.15+14.50, foo(false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(12.15+14.50, foo(false));
assertOptimized(foo);

assertEquals(12.15+14.50, foo(true));
assertUnoptimized(foo);
