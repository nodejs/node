// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.atan2 callsite.
  for (let i = 0; i < 5; i++);
  return Math.atan2;
}

function mycall(f, x, y) { return f(x, y); }

function foo(x, y) { return mycall(myfun(), x, y); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(Math.atan2(1, 1), foo(1, 1));
assertEquals(Math.atan2(0.5, 2), foo(0.5, 2));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(Math.atan2(1, 1), foo(1, 1));
assertEquals(Math.atan2(0.5, 2), foo(0.5, 2));
assertEquals(Math.atan2(-1, -1), foo(-1, -1));
assertEquals(NaN, foo(NaN, 1));
assertOptimized(foo);

// A non-number input deopts through the reduced eager-deopt path; result only --
// opt state after the deopt is not stable across stress variants.
assertEquals(NaN, foo({}, 1));
