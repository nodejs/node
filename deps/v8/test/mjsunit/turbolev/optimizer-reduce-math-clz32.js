// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.clz32 callsite.
  for (let i = 0; i < 5; i++);
  return Math.clz32;
}

function mycall(f, x) { return f(x); }

function foo(x) { return mycall(myfun(), x | 0); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(31, foo(1));
assertEquals(27, foo(16));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall(() => 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(31, foo(1));
assertEquals(27, foo(16));
assertEquals(32, foo(0));
assertEquals(0, foo(-1));
assertOptimized(foo);
