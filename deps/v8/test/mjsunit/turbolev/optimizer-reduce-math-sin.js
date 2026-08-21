// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.sin callsite.
  for (let i = 0; i < 5; i++);
  return Math.sin;
}

function mycall(f, x) { return f(x); }

function foo(x) { return mycall(myfun(), x); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(Math.sin(0.5), foo(0.5));
assertEquals(0, foo(0));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(Math.sin(0.5), foo(0.5));
assertEquals(0, foo(0));
assertEquals(Math.sin(1e30), foo(1e30));
assertEquals(NaN, foo(NaN));
assertOptimized(foo);

// A non-number input deopts through the reduced eager-deopt path; result only --
// opt state after the deopt is not stable across stress variants.
assertEquals(NaN, foo({}));
