// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-maglev-loop-peeling --turbofan
// Flags: --turbolev-escape-analysis

function bar() {}
%NeverOptimizeFunction(bar);

function foo(n) {
  let phi = 42;
  let o = { x: null };
  %AssertEscapeAnalysisElided(o);

  for (let i = 0; i < n; i++) {
    let inner = { y: i };
    o.x = null;
    o.x = inner;

    // We make the GraphBuilder and GraphOptimizer lose track of the value of
    // {o.x} by using an empty loop with loop peeling disabled, which triggers
    // an invalidation so {o.x} is loaded with a real LoadTaggedField below.
    for (let j = 0; j < 2; j++) {
      bar();
    }

    phi = o.x;
  }

  return phi.y;
}

%PrepareFunctionForOptimization(foo);
assertEquals(2, foo(3));
assertEquals(4, foo(5));

%OptimizeFunctionOnNextCall(foo);
assertEquals(2, foo(3));
assertEquals(4, foo(5));
assertOptimized(foo);
