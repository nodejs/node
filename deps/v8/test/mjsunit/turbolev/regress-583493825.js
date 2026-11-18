// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future

const other = { p: {} };

function foo(x) {
  let o = other;
  o.p = foo;
  function make() {
    return { a: x, p: x, c: x, d: x };
  }
  o = make();
  o.p = 2.5;
  return o.a + o.p;
}

%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 20; i++) {
  assertEquals(i + 3, foo(i + 0.5));
}
%OptimizeFunctionOnNextCall(foo);
assertEquals(6, foo(3.5));
