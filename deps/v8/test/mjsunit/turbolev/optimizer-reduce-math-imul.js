// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Big enough that the inliner picks `myfun` over the Math.imul callsite.
  for (let i = 0; i < 5; i++);
  return Math.imul;
}

function mycall(f, x, y) { return f(x, y); }

function foo(x, y) { return mycall(myfun(), x, y); }

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(12, foo(3, 4));
assertEquals(-6, foo(-2, 3));

// Polymorphic feedback on `f` so the optimizer pass has to reduce the call
// post-inline rather than at build time.
mycall((x, y) => x + y, 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(12, foo(3, 4));
assertEquals(20, foo(5, 4));
assertEquals(-6, foo(-2, 3));
// 32-bit truncating multiply, exercised through the reduced path.
assertEquals(-2, foo(0x7fffffff, 2));
assertOptimized(foo);

// A non-number argument is handled gracefully through the reduced path.
assertEquals(0, foo({}, 3));
