// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.abs callsite.
  for (let i = 0; i < 5; i++);
  return Math.abs;
}

function mycall(f, x) { return f(x); }

function foo(x) { return mycall(myfun(), x | 0); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(5, foo(5));
assertEquals(3, foo(-3));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(5, foo(5));
assertEquals(3, foo(-3));
assertEquals(0, foo(0));
assertOptimized(foo);

// Int32 abs of min_int is not representable as an int32; exercised through the
// reduced path. Result only -- opt state after the overflow deopt is not stable
// across stress variants.
assertEquals(0x80000000, foo(-0x80000000));
