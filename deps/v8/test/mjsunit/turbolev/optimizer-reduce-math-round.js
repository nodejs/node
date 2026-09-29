// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.round callsite.
  for (let i = 0; i < 5; i++);
  return Math.round;
}

function mycall(f, x) { return f(x); }

// Float64Round path: the typed-array load gives a statically-known number.
const f64 = new Float64Array(1);
function foo(x) { f64[0] = x; return mycall(myfun(), f64[0]); }

// Smi path: Math.round of an integer returns the integer directly.
function fooInt(x) { return mycall(myfun(), x | 0); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(fooInt);

assertEquals(2, foo(2.4));
assertEquals(3, foo(2.6));
assertEquals(7, fooInt(7));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0);

%OptimizeFunctionOnNextCall(foo);
%OptimizeFunctionOnNextCall(fooInt);
assertEquals(2, foo(2.4));
assertEquals(3, foo(2.6));
assertEquals(-2, foo(-2.4));
assertEquals(3, foo(2.5));
assertEquals(5, foo(5));
assertOptimized(foo);
assertEquals(7, fooInt(7));
assertEquals(-3, fooInt(-3));
assertOptimized(fooInt);
