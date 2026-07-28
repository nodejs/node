// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x += 5;
    bar.arguments[0].y += 3;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  // We create 2 fields with the same value, but {baz} will eventually set both
  // values to be different. Bugs in value numbering could merge the 2
  // HeapNumbers, which would lead to a wrong result after {baz}.
  o.x = 13.35;
  o.y = 13.35;
  baz(deopt);
}

function foo(deopt) {
  let o = { x : "abc", y : "def" };
  bar(o, deopt);
  %AssertEscapeAnalysisElided(o);
  return o.x + o.y;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(13.35+13.35, foo(false));
assertEquals(13.35+13.35, foo(false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(13.35+13.35, foo(false));
assertOptimized(foo);

assertEquals(13.35+13.35+5+3, foo(true));
assertUnoptimized(foo);
