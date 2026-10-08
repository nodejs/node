// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.max callsite.
  for (let i = 0; i < 5; i++);
  return Math.max;
}

function mycall(f, x, y) { return f(x, y); }

function foo(x, y) { return mycall(myfun(), x | 0, y | 0); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(2, foo(1, 2));
assertEquals(5, foo(5, 3));

// Polymorphic feedback on `f` so build-time target narrowing in `mycall`
// alone doesn't fold the call; the optimizer pass has to do it post-inline.
mycall((x, y) => x + y, 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(2, foo(1, 2));
assertEquals(5, foo(5, 3));
assertEquals(7, foo(7, 7));
assertEquals(-3, foo(-5, -3));
assertOptimized(foo);

// Float arguments still get truncated by `| 0`, so the Int32 path stays
// engaged.
assertEquals(1, foo(1.5, 0.5));
assertEquals(2, foo(0.5, 2.7));
