// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-maglev-loop-peeling --turbofan
// Flags: --turbolev-escape-analysis

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x += 10;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  baz(deopt);
}

function foo(x, b, deopt) {
  let o = { x : x };
  for (let i = 0; i < 42; i++) {
    if (b) {
      o.x = 17;
    } else {
      o.x = 29;
    }
  }
  bar(o, deopt);
  %AssertEscapeAnalysisElided(o);
  return o.x;
}

const kNoDeopt = false;
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(17, foo(42, true, kNoDeopt));
assertEquals(29, foo(42, false, kNoDeopt));

%OptimizeFunctionOnNextCall(foo);
assertEquals(17, foo(42, true, kNoDeopt));
assertEquals(29, foo(42, false, kNoDeopt));
assertOptimized(foo);

// Triggering deopt (which confirms that the object was escaped and that the
// deopt works).
assertEquals(17+10, foo(42, true, true));
assertUnoptimized(foo);
