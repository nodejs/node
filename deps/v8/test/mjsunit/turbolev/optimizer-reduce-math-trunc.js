// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.trunc callsite.
  for (let i = 0; i < 5; i++);
  return Math.trunc;
}

function mycall(f, x) { return f(x); }

function foo(x) { return mycall(myfun(), x); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(1, foo(1.7));
assertEquals(-1, foo(-1.7));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(1, foo(1.7));
assertEquals(-1, foo(-1.7));
assertEquals(3, foo(3));
assertOptimized(foo);

// A non-number input is handled gracefully whether or not the rounding op is
// reduced on this architecture.
assertEquals(NaN, foo({}));

%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo({}));
assertEquals(1, foo(1.7));
assertOptimized(foo);
