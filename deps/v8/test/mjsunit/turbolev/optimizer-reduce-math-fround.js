// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.fround callsite.
  for (let i = 0; i < 5; i++);
  return Math.fround;
}

function mycall(f, x) { return f(x); }

function foo(x) { return mycall(myfun(), x); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(1.5, foo(1.5));
assertEquals(Math.fround(0.1), foo(0.1));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(1.5, foo(1.5));
assertEquals(Math.fround(0.1), foo(0.1));
assertEquals(Math.fround(1e30), foo(1e30));
assertEquals(Infinity, foo(Infinity));
assertOptimized(foo);

// A non-number input is handled gracefully through the reduced path.
assertEquals(NaN, foo({}));
